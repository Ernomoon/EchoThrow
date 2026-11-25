#include "PluginEditor.h"
#include "Presets.h"

namespace et
{
using namespace theme;

namespace
{
    juce::RangedAudioParameter& par (EchoThrowProcessor& p, const char* id)
    {
        return *static_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
    }

    // ---- layout (design coordinates, 1000 x 690) ----
    const juce::Rectangle<float> waveCard { 16, 60, 968, 244 };
    const juce::Rectangle<float> timeCard { 16, 314, 462, 230 };
    const juce::Rectangle<float> eqCard   { 490, 314, 494, 230 };
    const juce::Rectangle<float> knobCard { 16, 554, 968, 124 };
    const juce::Rectangle<int>   waveWell { 28, 104, 944, 148 };
}

//==============================================================================
class EchoThrowEditor::Content : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    std::function<void (const juce::File&)> onFile;

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (isAudio (juce::File (f))) return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        for (auto& f : files)
            if (isAudio (juce::File (f))) { if (onFile) onFile (juce::File (f)); return; }
    }

private:
    static bool isAudio (const juce::File& f)
    {
        return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3;wave");
    }
};

class EchoThrowEditor::Backdrop : public juce::Component
{
public:
    Backdrop() { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (bg1, 0, 0, bg0, 0, b.getHeight(), false));
        g.fillAll();

        // top bar
        iconMark (g, { 26.0f, 8.0f, 44.0f, 40.0f }, cyan, amber);
        juce::AttributedString brand;
        brand.setJustification (juce::Justification::centredLeft);
        brand.append ("ECHO", font (17.0f, false, 0.34f), textMid);
        brand.append ("THROW", font (17.0f, true, 0.30f), textBright);
        brand.draw (g, juce::Rectangle<float> (78.0f, 8.0f, 200.0f, 40.0f));

        drawCard (g, waveCard);
        drawCard (g, timeCard);
        drawCard (g, eqCard);
        drawCard (g, knobCard);
        drawWell (g, waveWell.toFloat(), 10.0f);

        drawCapsText (g, "CAPTURE", { waveCard.getX() + 22.0f, waveCard.getY() + 8.0f, 150.0f, 34.0f }, textDim, 10.5f, juce::Justification::centredLeft, true, 0.26f);
        drawCapsText (g, "DELAY", { timeCard.getX() + 22.0f, timeCard.getY() + 8.0f, 150.0f, 22.0f }, textDim, 10.5f, juce::Justification::centredLeft, true, 0.26f);
        drawCapsText (g, "ECHO EQ", { eqCard.getX() + 22.0f, eqCard.getY() + 8.0f, 150.0f, 26.0f }, textDim, 10.5f, juce::Justification::centredLeft, true, 0.26f);

        // thin divider between the knob groups: time/feel | tone | output
        g.setColour (stroke.withAlpha (0.8f));
        for (int i : { 2, 6 })
            g.fillRect (28.0f + (float) i * 94.4f - 1.0f, knobCard.getY() + 16.0f, 1.0f, knobCard.getHeight() - 32.0f);
    }
};

class EchoThrowEditor::StatusText : public juce::Component
{
public:
    StatusText() { setInterceptsMouseClicks (false, false); }
    void set (const juce::String& t, juce::Colour c)
    {
        if (t != text || c != colour) { text = t; colour = c; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (colour);
        g.setFont (font (10.0f, false, 0.12f));
        g.drawText (text, getLocalBounds().toFloat(), juce::Justification::centred, true);
    }
private:
    juce::String text;
    juce::Colour colour = textDim;
};

//==============================================================================
EchoThrowEditor::EchoThrowEditor (EchoThrowProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      modeSwitch (par (p, pid::pingPong), "STEREO", "PING-PONG"),
      wave (p),
      recButton (p),
      listenButton (p),
      loopToggle (par (p, pid::loop), "LOOP", ParamToggle::Icon::Loop),
      tailValue (par (p, pid::tail), "MAX TAIL"),
      timeKnob (par (p, pid::timeL), "TIME", Knob::Style::Hero),
      timeRKnob (par (p, pid::timeR), "TIME R", Knob::Style::Small),
      syncToggle (par (p, pid::sync), "SYNC TO HOST"),
      linkToggle (par (p, pid::link), "LINK L / R"),
      muteToggle (par (p, pid::muteLive), "MUTE LIVE SIGNAL"),
      hearDryToggle (par (p, pid::hearDry), "HEAR DRY"),
      exportDryToggle (par (p, pid::exportDry), "EXPORT WITH DRY"),
      feedbackKnob (par (p, pid::feedback), "FEEDBACK", Knob::Style::Small),
      widthKnob (par (p, pid::width), "WIDTH", Knob::Style::Small),
      driveKnob (par (p, pid::drive), "DRIVE", Knob::Style::Small),
      modKnob (par (p, pid::mod), "MOD", Knob::Style::Small),
      rateKnob (par (p, pid::modRate), "RATE", Knob::Style::Small),
      diffKnob (par (p, pid::diffusion), "DIFFUSE", Knob::Style::Small),
      lowCutKnob (par (p, pid::lowCut), "LOW CUT", Knob::Style::Small),
      highCutKnob (par (p, pid::highCut), "HIGH CUT", Knob::Style::Small),
      levelKnob (par (p, pid::wetLevel), "ECHO LEVEL", Knob::Style::Small),
      eqView (p),
      eqToggle (par (p, pid::eqOn), "ON")
{
    content = std::make_unique<Content>();
    backdrop = std::make_unique<Backdrop>();
    status = std::make_unique<StatusText>();
    addAndMakeVisible (*content);

    content->addAndMakeVisible (*backdrop);
    backdrop->setBounds (0, 0, kDesignWidth, kDesignHeight);
    backdrop->setBufferedToImage (true);

    auto add = [this] (juce::Component& c, juce::Rectangle<int> r) { content->addAndMakeVisible (c); c.setBounds (r); };

    // ---- top bar ----
    add (presetBar, { 341, 11, 270, 34 });
    add (saveButton, { 619, 11, 40, 34 });
    add (menuButton, { 940, 11, 40, 34 });
    presetBar.setName (proc.presetName);
    presetBar.onOpenMenu = [this] { showPresetMenu(); };
    presetBar.onPrev = [this] { stepPreset (-1); };
    presetBar.onNext = [this] { stepPreset (+1); };
    saveButton.onClick = [this] { promptSavePreset(); };
    menuButton.onClick = [this] { showMainMenu(); };

    // ---- capture card ----
    add (modeSwitch, { 380, 66, 240, 30 });
    add (resetButton, { 872, 66, 96, 30 });
    resetButton.onClick = [this]
    {
        resetSoundParameters (proc.apvts);
        proc.presetName = "Default";
        presetBar.setName ("Default");
    };
    add (wave, waveWell);
    add (recButton, { 32, 262, 108, 32 });
    add (listenButton, { 148, 262, 116, 32 });
    add (loopToggle, { 272, 262, 90, 32 });
    add (*status, { 372, 262, 296, 32 });
    add (tailValue, { 676, 262, 100, 32 });
    add (dragPill, { 780, 262, 188, 32 });
    recButton.onClick = [this] { onRecClicked(); };
    listenButton.onClick = [this] { onListenClicked(); };
    dragPill.isReady = [this] { return proc.getRenderedClip() != nullptr; };
    dragPill.getFile = [this] { return proc.exportCurrentClip(); };

    // ---- delay card ----
    add (timeKnob, { 24, 334, 236, 206 });
    add (syncToggle, { 286, 336, 176, 30 });
    add (linkToggle, { 286, 375, 176, 30 });
    add (muteToggle, { 286, 414, 176, 30 });
    add (hearDryToggle, { 286, 453, 176, 30 });
    add (exportDryToggle, { 286, 492, 176, 30 });

    // ---- EQ card ----
    add (eqView, { 498, 350, 478, 186 });
    add (eqToggle, { 898, 320, 72, 26 });

    // ---- knob strip ----
    Knob* strip[] = { &timeRKnob, &feedbackKnob, &widthKnob, &driveKnob, &modKnob, &rateKnob, &diffKnob, &lowCutKnob, &highCutKnob, &levelKnob };
    for (int i = 0; i < 10; ++i)
        add (*strip[i], { (int) std::lround (28.0 + i * 94.4), 566, 94, 104 });

    // ---- the TIME knobs follow SYNC / LINK ----
    syncOn = par (p, pid::sync).getValue() > 0.5f;
    linkOn = par (p, pid::link).getValue() > 0.5f;
    timeKnob.valueText = [this] (float v) -> juce::String
    {
        if (syncOn)
        {
            const int idx = juce::jlimit (0, kNumDivisions - 1, (int) std::lround (v));
            const double ms = divisionSeconds (idx, proc.getHostBpm()) * 1000.0;
            return juce::String (syncDivisions()[(size_t) idx].name) + "  (" + formatMs ((float) ms) + ")";
        }
        return formatMs (v);
    };
    timeRKnob.valueText = [this] (float v) -> juce::String
    {
        if (syncOn)
            return syncDivisions()[(size_t) juce::jlimit (0, kNumDivisions - 1, (int) std::lround (v))].name;
        return formatMs (v);
    };
    syncAttachment = std::make_unique<juce::ParameterAttachment> (par (p, pid::sync), [this] (float v) { syncOn = v > 0.5f; updateTimeKnobs(); });
    linkAttachment = std::make_unique<juce::ParameterAttachment> (par (p, pid::link), [this] (float v) { linkOn = v > 0.5f; updateTimeKnobs(); });
    syncAttachment->sendInitialUpdate();
    linkAttachment->sendInitialUpdate();

    content->onFile = [this] (const juce::File& f)
    {
        if (proc.loadClipFromFile (f)) toast ("LOADED  " + f.getFileName().toUpperCase());
        else                           toast ("COULD NOT READ THAT FILE");
    };

    // ---- window ----
    const int savedWidth = proc.uiWidth;     // resized() overwrites uiWidth while the limits are being set up
    setResizable (true, true);
    setResizeLimits (580, (int) (580.0 * kDesignHeight / kDesignWidth), 2000, (int) (2000.0 * kDesignHeight / kDesignWidth));
    getConstrainer()->setFixedAspectRatio ((double) kDesignWidth / (double) kDesignHeight);
    const int w = juce::jlimit (580, 2000, savedWidth);
    setSize (w, (int) std::lround ((double) w * kDesignHeight / kDesignWidth));
    constructed = true;
    proc.uiWidth = getWidth();

    startTimerHz (30);
}

EchoThrowEditor::~EchoThrowEditor()
{
    stopTimer();
    syncAttachment.reset();
    linkAttachment.reset();
}

void EchoThrowEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg0);
}

void EchoThrowEditor::resized()
{
    const float s = (float) getWidth() / (float) kDesignWidth;
    content->setTransform (juce::AffineTransform::scale (s));
    content->setBounds (0, 0, kDesignWidth, kDesignHeight);
    if (constructed)
        proc.uiWidth = getWidth();
}

void EchoThrowEditor::setUiScale (float scale)
{
    setSize ((int) std::lround (kDesignWidth * scale), (int) std::lround (kDesignHeight * scale));
}

//==============================================================================
void EchoThrowEditor::updateTimeKnobs()
{
    const bool sync = syncOn, link = linkOn;

    timeKnob.setParameter (par (proc, sync ? pid::divL : pid::timeL));
    timeKnob.setSteps (sync ? kNumDivisions : 0);
    timeKnob.setName (link ? "TIME" : "TIME L");

    timeRKnob.setParameter (par (proc, sync ? pid::divR : pid::timeR));
    timeRKnob.setSteps (sync ? kNumDivisions : 0);
    timeRKnob.setDimmed (link);
    timeRKnob.setInterceptsMouseClicks (! link, ! link);
}

void EchoThrowEditor::toast (const juce::String& t, int ms)
{
    toastText = t;
    toastUntil = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
}

void EchoThrowEditor::onRecClicked()
{
    proc.toggleRecord();
}

void EchoThrowEditor::onListenClicked()
{
    if (! proc.hasClip())
    {
        toast ("NOTHING TO PLAY YET  -  PRESS REC FIRST", 3500);
        return;
    }
    if (proc.isListening())
    {
        proc.stopListening();
        awaitingListenStart = false;
        return;
    }
    heartbeatAtRequest = proc.getHeartbeat();
    listenRequestedAt = juce::Time::getMillisecondCounter();
    awaitingListenStart = true;
    proc.startListening();
}

void EchoThrowEditor::timerCallback()
{
    wave.refresh();
    eqView.refresh();

    const auto st = proc.getCaptureState();
    if (st != lastCaptureState || st == CaptureEngine::State::Armed || st == CaptureEngine::State::Recording)
        recButton.repaint();
    lastCaptureState = st;

    const bool listening = proc.isListening(), hasClip = proc.hasClip();
    if (listening != lastListening || hasClip != lastHadClip)
        listenButton.repaint();
    lastListening = listening;
    lastHadClip = hasClip;

    const bool ready = proc.getRenderedClip() != nullptr;
    if (ready != lastReady) { lastReady = ready; dragPill.repaint(); }

    if (syncOn)
    {
        const double bpm = proc.getHostBpm();
        if (std::abs (bpm - lastBpm) > 0.01) { lastBpm = bpm; timeKnob.repaint(); }
    }

    const auto now = juce::Time::getMillisecondCounter();

    // LISTEN pressed but the host is not calling us: tell the user how to fix it instead of staying silent.
    if (awaitingListenStart)
    {
        if (listening)
            awaitingListenStart = false;
        else if (now - listenRequestedAt > 700)
        {
            awaitingListenStart = false;
            if (proc.getHeartbeat() == heartbeatAtRequest)
                toast ("HOST IS NOT PROCESSING AUDIO  -  IN FL STUDIO OPEN THE PLUGIN'S GEAR MENU AND UNTICK 'SMART DISABLE'", 12000);
        }
    }

    // status line
    juce::String text;
    juce::Colour colour = textDim;
    if (now < toastUntil)
    {
        text = toastText;
        colour = amber;
    }
    else if (st == CaptureEngine::State::Recording)
    {
        text = "RECORDING  -  PRESS REC WHEN THE WORD ENDS";
        colour = recRed;
    }
    else if (st == CaptureEngine::State::Armed)
    {
        text = "ARMED  -  WAITING FOR THE FIRST SOUND";
        colour = amber;
    }
    else if (auto clip = proc.getRenderedClip())
    {
        const double sr = clip->sampleRate;
        text = "CLIP " + juce::String ((double) clip->srcLen / sr, 2) + " s   |   TAIL "
               + juce::String ((double) (clip->outLen - clip->srcLen) / sr, 2) + " s";
        const auto pos = proc.describePosition (proc.getTrimStart());
        if (pos.isNotEmpty()) text << "   |   " << pos;
        colour = textMid;
    }
    else if (proc.hasClip())
    {
        text = "RENDERING...";
        colour = textMid;
    }
    else
    {
        text = "NO CLIP YET";
    }
    status->set (text, colour);
}

//==============================================================================
// Presets & menus
//==============================================================================
juce::StringArray EchoThrowEditor::allPresetNames() const
{
    juce::StringArray names;
    for (auto& p : factoryPresets()) names.add (p.name);
    names.addArray (listUserPresets());
    return names;
}

void EchoThrowEditor::applyPresetByIndex (int index)
{
    const auto names = allPresetNames();
    if (! juce::isPositiveAndBelow (index, names.size())) return;

    if (index < (int) factoryPresets().size())
        applyFactoryPreset (proc.apvts, index);
    else
        loadUserPreset (proc.apvts, names[index]);

    proc.presetName = names[index];
    presetBar.setName (names[index]);
}

void EchoThrowEditor::stepPreset (int delta)
{
    const auto names = allPresetNames();
    int cur = names.indexOf (proc.presetName);
    if (cur < 0) cur = delta > 0 ? -1 : 0;
    applyPresetByIndex ((cur + delta + names.size()) % names.size());
}

void EchoThrowEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const auto& factory = factoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
    {
        juce::PopupMenu::Item item (factory[(size_t) i].name);
        item.setTicked (proc.presetName == factory[(size_t) i].name);
        item.setAction ([this, i] { applyPresetByIndex (i); });
        m.addItem (item);
    }

    const auto users = listUserPresets();
    if (users.size() > 0)
    {
        m.addSeparator();
        m.addSectionHeader ("User presets");
        for (int j = 0; j < users.size(); ++j)
        {
            juce::PopupMenu::Item item (users[j]);
            item.setTicked (proc.presetName == users[j]);
            const int idx = (int) factory.size() + j;
            item.setAction ([this, idx] { applyPresetByIndex (idx); });
            m.addItem (item);
        }
    }

    m.addSeparator();
    m.addItem ("Save preset...", [this] { promptSavePreset(); });
    m.addItem ("Open presets folder", [] { auto f = getUserPresetFolder(); f.createDirectory(); f.revealToUser(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBar).withMinimumWidth (240));
}

void EchoThrowEditor::promptSavePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Give your preset a name:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presetName == "Default" ? juce::String() : proc.presetName);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<juce::AlertWindow> safeWindow (w);
    juce::Component::SafePointer<EchoThrowEditor> safeThis (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safeWindow, safeThis] (int result)
    {
        if (result != 1 || safeWindow == nullptr || safeThis == nullptr) return;
        const auto name = safeWindow->getTextEditorContents ("name").trim();
        if (name.isNotEmpty() && saveUserPreset (safeThis->proc.apvts, name))
        {
            safeThis->proc.presetName = juce::File::createLegalFileName (name);
            safeThis->presetBar.setName (safeThis->proc.presetName);
            safeThis->toast ("PRESET SAVED  " + safeThis->proc.presetName.toUpperCase(), 3500);
        }
    }), true);
}

void EchoThrowEditor::showMainMenu()
{
    juce::PopupMenu m, sizes;
    for (float s : { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f })
    {
        juce::PopupMenu::Item item (juce::String (juce::roundToInt (s * 100.0f)) + "%");
        item.setTicked (std::abs ((float) getWidth() / (float) kDesignWidth - s) < 0.02f);
        item.setAction ([this, s] { setUiScale (s); });
        sizes.addItem (item);
    }
    m.addSubMenu ("Window size", sizes);
    m.addSeparator();
    m.addItem ("Open exports folder", []
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EchoThrow").getChildFile ("Exports");
        f.createDirectory();
        f.revealToUser();
    });
    m.addItem ("Open presets folder", [] { auto f = getUserPresetFolder(); f.createDirectory(); f.revealToUser(); });
    m.addSeparator();
    m.addItem ("Clear clip", proc.hasClip(), false, [this] { proc.clearClip(); });
    m.addItem ("About EchoThrow", [this]
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "EchoThrow",
            "Version 1.0.0\n\nRecord the end of a word, shape a delay for it, then drag the result onto your playlist.\n"
            "Built with JUCE.", "OK", this);
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton));
}

} // namespace et
