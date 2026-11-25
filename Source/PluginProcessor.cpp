#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace et
{
namespace
{
    juce::MemoryBlock encodeFlac (const juce::AudioBuffer<float>& buf, double sampleRate)
    {
        juce::MemoryBlock block;
        auto* stream = new juce::MemoryOutputStream (block, false);
        juce::FlacAudioFormat flac;
        std::unique_ptr<juce::AudioFormatWriter> writer (flac.createWriterFor (stream, sampleRate, 2, 24, {}, 5));
        if (writer == nullptr)
        {
            delete stream;
            return {};
        }
        writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        writer.reset();   // finishes the file and deletes the stream
        return block;
    }

    bool decodeFlac (const juce::MemoryBlock& block, juce::AudioBuffer<float>& dest, double& sampleRate)
    {
        juce::FlacAudioFormat flac;
        std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (new juce::MemoryInputStream (block, false), true));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > 60 * 192000)
            return false;
        dest.setSize (2, (int) reader->lengthInSamples);
        reader->read (&dest, 0, (int) reader->lengthInSamples, 0, true, true);
        sampleRate = reader->sampleRate;
        return true;
    }

    bool isRenderParam (const juce::String& id)
    {
        return id != pid::hearDry && id != pid::exportDry && id != pid::muteLive && id != pid::loop;
    }
}

//==============================================================================
EchoThrowProcessor::EchoThrowProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw = { get (pid::sync), get (pid::link), get (pid::timeL), get (pid::timeR), get (pid::divL), get (pid::divR),
            get (pid::feedback), get (pid::pingPong), get (pid::width), get (pid::lowCut), get (pid::highCut),
            get (pid::drive), get (pid::mod), get (pid::modRate), get (pid::diffusion), get (pid::wetLevel), get (pid::tail),
            get (pid::hearDry), get (pid::exportDry), get (pid::muteLive), get (pid::loop),
            get (pid::eqOn), get (pid::eqHp), get (pid::eqLp), get (pid::eqLsF), get (pid::eqLsG),
            get (pid::eq1F), get (pid::eq1G), get (pid::eq1Q), get (pid::eq2F), get (pid::eq2G), get (pid::eq2Q),
            get (pid::eqHsF), get (pid::eqHsG) };

    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isRenderParam (r->paramID))
                apvts.addParameterListener (r->paramID, this);

    startTimerHz (30);
}

EchoThrowProcessor::~EchoThrowProcessor()
{
    stopTimer();
    for (auto* p : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isRenderParam (r->paramID))
                apvts.removeParameterListener (r->paramID, this);
}

juce::AudioProcessorEditor* EchoThrowProcessor::createEditor()
{
    return new EchoThrowEditor (*this);
}

bool EchoThrowProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono());
}

void EchoThrowProcessor::parameterChanged (const juce::String&, float)
{
    paramsDirty.store (true);
}

//==============================================================================
void EchoThrowProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    capture.prepare (sampleRate);
    playing = false;
    playPos = 0;
    fadeOutLeft = -1;
    playPosition.store (-1);
    playCommand.store (0);
    paramsDirty.store (true);   // re-render at the new sample rate (done on the message thread)
}

void EchoThrowProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    heartbeat.fetch_add (1, std::memory_order_relaxed);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ---- host tempo / position ----
    double ppq = std::nan ("");
    double bpm = hostBpm.load (std::memory_order_relaxed);
    if (auto* head = getPlayHead())
    {
        if (auto position = head->getPosition())
        {
            if (auto b = position->getBpm())
            {
                bpm = *b;
                if (std::abs (bpm - hostBpm.load (std::memory_order_relaxed)) > 0.005)
                {
                    hostBpm.store (bpm, std::memory_order_relaxed);
                    if (*raw.sync > 0.5f)
                        paramsDirty.store (true);
                }
                hostBpmValid.store (true, std::memory_order_relaxed);
            }
            if (auto p = position->getPpqPosition())
                ppq = *p;
            if (auto ts = position->getTimeSignature())
                hostBeatsPerBar.store ((double) ts->numerator * 4.0 / (double) std::max (1, ts->denominator), std::memory_order_relaxed);
        }
    }

    // ---- 1. record the incoming (dry) signal. The signal itself always passes through untouched. ----
    capture.process (buffer, ppq, bpm);

    // ---- 2. preview playback of the rendered clip ----
    switch (playCommand.exchange (0))
    {
        case 1:  playing = true; playPos = 0; fadeOutLeft = -1; break;
        case 2:  if (playing && fadeOutLeft < 0) fadeOutLeft = kFadeOutSamples; break;
        default: break;
    }

    if (playing)
    {
        const RenderedClip* clip = renderer.acquire();
        if (clip != nullptr && clip->outLen > 0 && std::abs (clip->sampleRate - currentSampleRate) < 0.5)
        {
            if (*raw.muteLive > 0.5f)
                buffer.clear();

            const bool wantDry = *raw.hearDry > 0.5f;
            const bool loop = *raw.loop > 0.5f;
            const float* wl = clip->wet.getReadPointer (0);
            const float* wr = clip->wet.getReadPointer (1);
            const float* dl = clip->dry.getReadPointer (0);
            const float* dr = clip->dry.getReadPointer (1);
            float* out0 = buffer.getWritePointer (0);
            float* out1 = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
            const int len = clip->outLen, srcLen = clip->srcLen;

            for (int i = 0; i < numSamples; ++i)
            {
                if (playPos >= len)
                {
                    if (loop) playPos = 0;
                    else { playing = false; break; }
                }

                float l = wl[playPos], r = wr[playPos];
                if (wantDry && playPos < srcLen)
                {
                    l += dl[playPos];
                    r += dr[playPos];
                }

                if (fadeOutLeft >= 0)
                {
                    const float g = (float) fadeOutLeft / (float) kFadeOutSamples;
                    l *= g; r *= g;
                    if (--fadeOutLeft < 0) { playing = false; }
                }

                if (out1 != nullptr) { out0[i] += l; out1[i] += r; }
                else                 { out0[i] += 0.5f * (l + r); }

                ++playPos;
                if (! playing) break;
            }
        }
        else if (clip == nullptr)
        {
            playing = false;
        }
        renderer.release();
    }

    playPosition.store (playing ? playPos : -1, std::memory_order_relaxed);
}

//==============================================================================
void EchoThrowProcessor::timerCallback()
{
    pollCapture();

    if (paramsDirty.exchange (false))
        requestRender();

    if (encodedDirty && capture.getState() == CaptureEngine::State::Idle)
        updateEncodedClip();
}

void EchoThrowProcessor::pollCapture()
{
    juce::AudioBuffer<float> take;
    double ppq = 0.0, bpm = 0.0;
    if (capture.collectTake (take, ppq, bpm))
    {
        {
            const juce::ScopedLock sl (clipLock);
            captureStartPpq = ppq;
            captureBpm = bpm;
            captureBeatsPerBar = hostBeatsPerBar.load();
        }
        setSource (std::move (take), currentSampleRate, true);
    }
}

void EchoThrowProcessor::toggleRecord()
{
    switch (capture.getState())
    {
        case CaptureEngine::State::Idle:
            stopListening();
            capture.arm();
            break;
        case CaptureEngine::State::Armed:
        case CaptureEngine::State::Recording:
            capture.stop();
            break;
        case CaptureEngine::State::Finished:
            pollCapture();
            break;
    }
}

void EchoThrowProcessor::startListening()
{
    if (hasClip())
        playCommand.store (1);
}

void EchoThrowProcessor::stopListening()
{
    if (isListening())
        playCommand.store (2);
}

//==============================================================================
void EchoThrowProcessor::setSource (juce::AudioBuffer<float>&& audio, double rate, bool autoTrim)
{
    const juce::ScopedLock sl (clipLock);

    const int n = audio.getNumSamples();
    int tStart = 0, tEnd = n;

    if (autoTrim && n > 1 && rate > 0.0)
    {
        const float threshold = 0.003f;
        int first = -1, last = -1;
        for (int i = 0; i < n && first < 0; ++i)
            if (std::max (std::abs (audio.getSample (0, i)), std::abs (audio.getSample (1, i))) > threshold) first = i;
        for (int i = n - 1; i >= 0 && last < 0; --i)
            if (std::max (std::abs (audio.getSample (0, i)), std::abs (audio.getSample (1, i))) > threshold) last = i;
        if (first >= 0 && last > first)
        {
            tStart = std::max (0, first - (int) (0.02 * rate));
            tEnd = std::min (n, last + (int) (0.08 * rate));
        }
    }

    source = std::make_shared<const juce::AudioBuffer<float>> (std::move (audio));
    sourceRate = rate;
    trimStart = tStart;
    trimEnd = std::max (tStart + 1, tEnd);
    encodedDirty = true;
    paramsDirty.store (true);
}

void EchoThrowProcessor::setTrim (int start, int end)
{
    const juce::ScopedLock sl (clipLock);
    if (! source) return;
    const int n = source->getNumSamples();
    const int minLen = std::max (64, (int) (0.01 * sourceRate));
    start = juce::jlimit (0, std::max (0, n - minLen), start);
    end = juce::jlimit (start + minLen, n, end);
    if (start == trimStart && end == trimEnd) return;
    trimStart = start;
    trimEnd = end;
    paramsDirty.store (true);
}

void EchoThrowProcessor::resetTrim()
{
    const juce::ScopedLock sl (clipLock);
    if (! source) return;
    setTrim (0, source->getNumSamples());
}

void EchoThrowProcessor::clearClip()
{
    stopListening();
    {
        const juce::ScopedLock sl (clipLock);
        source.reset();
        trimStart = trimEnd = 0;
        encodedClip.reset();
        encodedDirty = false;
        captureStartPpq = std::nan ("");
    }
    renderer.submit ({});
}

bool EchoThrowProcessor::loadClipFromFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
        return false;

    const int n = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (CaptureEngine::kMaxSeconds * reader->sampleRate));
    juce::AudioBuffer<float> audio (2, n);
    audio.clear();
    reader->read (&audio, 0, n, 0, true, true);
    if (reader->numChannels == 1)
        audio.copyFrom (1, 0, audio, 0, 0, n);

    stopListening();
    {
        const juce::ScopedLock sl (clipLock);
        captureStartPpq = std::nan ("");
    }
    setSource (std::move (audio), reader->sampleRate, false);
    return true;
}

void EchoThrowProcessor::resampleSourceIfNeeded()
{
    // called with clipLock held
    if (! source || currentSampleRate <= 0.0 || sourceRate <= 0.0 || std::abs (sourceRate - currentSampleRate) < 0.5)
        return;

    const double ratio = currentSampleRate / sourceRate;
    const int inLen = source->getNumSamples();
    const int outLen = std::max (1, (int) std::round ((double) inLen * ratio));
    juce::AudioBuffer<float> out (2, outLen);
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::LagrangeInterpolator interp;
        interp.process (sourceRate / currentSampleRate, source->getReadPointer (ch), out.getWritePointer (ch), outLen);
    }
    trimStart = (int) std::round ((double) trimStart * ratio);
    trimEnd = std::min (outLen, (int) std::round ((double) trimEnd * ratio));
    trimEnd = std::max (trimEnd, trimStart + 1);
    source = std::make_shared<const juce::AudioBuffer<float>> (std::move (out));
    sourceRate = currentSampleRate;
    encodedDirty = true;
}

DelayParams EchoThrowProcessor::makeDelayParams() const
{
    DelayParams p;
    p.sampleRate = currentSampleRate;

    const bool sync = *raw.sync > 0.5f;
    const bool link = *raw.link > 0.5f;
    const double bpm = hostBpm.load();

    auto seconds = [&] (bool right)
    {
        const bool useR = right && ! link;
        if (sync)
            return divisionSeconds ((int) std::lround (useR ? *raw.divR : *raw.divL), bpm);
        return (double) (useR ? *raw.timeR : *raw.timeL) * 0.001;
    };
    p.timeLSec = seconds (false);
    p.timeRSec = seconds (true);

    p.feedback = *raw.feedback;
    p.pingPong = *raw.pingPong > 0.5f;
    p.width = *raw.width;
    p.loopLowCutHz = *raw.lowCut;
    p.loopHighCutHz = *raw.highCut;
    p.drive = *raw.drive;
    p.modDepth = *raw.mod;
    p.modRateHz = *raw.modRate;
    p.diffusion = *raw.diffusion;
    p.wetGainDb = *raw.wetLevel;
    p.tailMaxSec = *raw.tail;

    p.eq.on  = *raw.eqOn > 0.5f;
    p.eq.hpF = *raw.eqHp;   p.eq.lpF = *raw.eqLp;
    p.eq.lsF = *raw.eqLsF;  p.eq.lsG = *raw.eqLsG;
    p.eq.b1F = *raw.eq1F;   p.eq.b1G = *raw.eq1G;  p.eq.b1Q = *raw.eq1Q;
    p.eq.b2F = *raw.eq2F;   p.eq.b2G = *raw.eq2G;  p.eq.b2Q = *raw.eq2Q;
    p.eq.hsF = *raw.eqHsF;  p.eq.hsG = *raw.eqHsG;
    return p;
}

void EchoThrowProcessor::requestRender()
{
    const juce::ScopedLock sl (clipLock);

    if (currentSampleRate <= 0.0)
    {
        paramsDirty.store (true);     // not prepared yet: try again on the next tick
        return;
    }

    resampleSourceIfNeeded();

    RenderManager::Request req;
    if (source)
    {
        req.source = source;
        req.start = trimStart;
        req.end = trimEnd;
        req.params = makeDelayParams();
    }
    renderer.submit (std::move (req));
}

//==============================================================================
juce::File EchoThrowProcessor::exportCurrentClip()
{
    auto clip = renderer.latest();
    if (clip == nullptr || clip->outLen <= 0)
        return {};

    const bool withDry = *raw.exportDry > 0.5f;
    if (clip->id == lastExportId && withDry == lastExportDry && lastExportFile.existsAsFile())
        return lastExportFile;

    juce::AudioBuffer<float> mix (2, clip->outLen);
    for (int ch = 0; ch < 2; ++ch)
    {
        mix.copyFrom (ch, 0, clip->wet, ch, 0, clip->outLen);
        if (withDry)
            mix.addFrom (ch, 0, clip->dry, ch, 0, std::min (clip->srcLen, clip->outLen));
    }

    auto tryFolder = [&] (const juce::File& folder) -> juce::File
    {
        if (! folder.createDirectory().wasOk())
            return {};
        auto file = folder.getNonexistentChildFile ("EchoThrow_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S")
                                                        + (withDry ? "_dry+echo" : "_echo"), ".wav");
        auto stream = std::make_unique<juce::FileOutputStream> (file);
        if (! stream->openedOk())
            return {};
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), clip->sampleRate, 2, 32, {}, 0));
        if (writer == nullptr)
            return {};
        stream.release();   // the writer owns it now
        writer->writeFromAudioSampleBuffer (mix, 0, mix.getNumSamples());
        writer.reset();
        return file;
    };

    juce::File result = tryFolder (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                       .getChildFile ("EchoThrow").getChildFile ("Exports"));
    if (! result.existsAsFile())
        result = tryFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("EchoThrow"));

    if (result.existsAsFile())
    {
        lastExportFile = result;
        lastExportId = clip->id;
        lastExportDry = withDry;
    }
    return result;
}

juce::String EchoThrowProcessor::describePosition (int sampleInSource) const
{
    const juce::ScopedLock sl (clipLock);
    if (! std::isfinite (captureStartPpq) || captureBpm <= 0.0 || sourceRate <= 0.0)
        return {};

    const double ppq = captureStartPpq + (double) sampleInSource / sourceRate * captureBpm / 60.0;
    if (ppq < 0.0)
        return {};

    const double bpb = std::max (1.0, captureBeatsPerBar);
    const int bar = (int) std::floor (ppq / bpb) + 1;
    const double beat = std::fmod (ppq, bpb) + 1.0;
    return "BAR " + juce::String (bar) + "  BEAT " + juce::String (beat, 2);
}

void EchoThrowProcessor::updateEncodedClip()
{
    const juce::ScopedLock sl (clipLock);
    encodedClip.reset();
    if (source)
        encodedClip = encodeFlac (*source, sourceRate);
    encodedDirty = false;
}

//==============================================================================
void EchoThrowProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("EchoThrowState");
    root.setAttribute ("version", 1);
    root.setAttribute ("uiWidth", uiWidth);
    root.setAttribute ("preset", presetName);

    if (auto stateXml = apvts.copyState().createXml())
        root.addChildElement (stateXml.release());

    {
        const juce::ScopedLock sl (clipLock);
        if (source)
        {
            if (encodedDirty)
            {
                encodedClip = encodeFlac (*source, sourceRate);
                encodedDirty = false;
            }
            if (encodedClip.getSize() > 0)
            {
                auto* c = root.createNewChildElement ("Clip");
                c->setAttribute ("rate", sourceRate);
                c->setAttribute ("trimStart", trimStart);
                c->setAttribute ("trimEnd", trimEnd);
                c->setAttribute ("bpm", captureBpm);
                c->setAttribute ("beatsPerBar", captureBeatsPerBar);
                if (std::isfinite (captureStartPpq))
                    c->setAttribute ("startPpq", captureStartPpq);
                c->addTextElement (juce::Base64::toBase64 (encodedClip.getData(), encodedClip.getSize()));
            }
        }
    }

    copyXmlToBinary (root, dest);
}

void EchoThrowProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("EchoThrowState"))
        return;

    uiWidth = juce::jlimit (500, 2400, xml->getIntAttribute ("uiWidth", kDesignWidth));
    presetName = xml->getStringAttribute ("preset", "Default");

    if (auto* ps = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*ps));

    if (auto* c = xml->getChildByName ("Clip"))
    {
        juce::MemoryOutputStream decoded;
        juce::AudioBuffer<float> audio;
        double rate = c->getDoubleAttribute ("rate", 48000.0);
        if (juce::Base64::convertFromBase64 (decoded, c->getAllSubText().trim())
            && decodeFlac (juce::MemoryBlock (decoded.getData(), decoded.getDataSize()), audio, rate))
        {
            const int n = audio.getNumSamples();
            setSource (std::move (audio), rate, false);
            const juce::ScopedLock sl (clipLock);
            trimStart = juce::jlimit (0, std::max (0, n - 1), c->getIntAttribute ("trimStart", 0));
            trimEnd = juce::jlimit (trimStart + 1, n, c->getIntAttribute ("trimEnd", n));
            captureBpm = c->getDoubleAttribute ("bpm", 0.0);
            captureBeatsPerBar = c->getDoubleAttribute ("beatsPerBar", 4.0);
            captureStartPpq = c->hasAttribute ("startPpq") ? c->getDoubleAttribute ("startPpq") : std::nan ("");
        }
    }
    else
    {
        clearClip();
    }
    paramsDirty.store (true);
}

} // namespace et
