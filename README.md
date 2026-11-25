# EchoThrow

VST3 delay-throw plugin: record the tail of a vocal word, shape a tempo-synced delay with a post-delay EQ,
audition it, then drag the rendered echo straight into your DAW playlist.

- Windows and macOS (universal) VST3, built with JUCE 8 + CMake.
- Installers are produced by GitHub Actions (see `docs/GITHUB_GUIDE_RU.md`, in Russian).
- Local build: `cmake -S . -B build && cmake --build build --config Release --target EchoThrow_VST3`
- Tests: `cmake -S . -B build -DECHOTHROW_BUILD_TESTS=ON`

Licensed under AGPL-3.0 (see `LICENSE`).
