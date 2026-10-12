# NAM Audio Processor — Full integration prototype

Modern-minimal JUCE VST3/Standalone project that integrates the official NeuralAmpModelerCore API.

## What is implemented in source

- CMake fetches JUCE 8.0.6 and NeuralAmpModelerCore v0.6.0. Since upstream v0.6.0 is organized around source-built command-line tools rather than a public CMake library target, this project compiles NAM Core sources into a local `namcore` static target.
- VST3 and Standalone targets.
- Calls `juce_generate_juce_header(NAMAudioProcessor)` to generate `JuceHeader.h`, fixing the MSVC C1083 error shown in the previous GitHub Actions run.
- NAM file selection and asynchronous model loading using `nam::get_dsp(path)`.
- Calls `Reset(sampleRate, maxBlockSize)` on the loader thread, not inside the audio callback.
- Mono-in/mono-out NAM model processing through `DSP::process()`.
- Input gain, output gain, dry/wet mix, bypass.
- Input/output peak readouts, level bars and clip latch.
- Saves plugin parameters and selected model path in host state.

## Important limitations / not yet verified

This project is a serious integration prototype, not a certified production release. It has **not been built or tested in this environment**. Upstream NAM Core is compiled from its source files into a local static target by this project's CMake. The upstream repository layout or dependency list may change between tags, so verify the include paths and source list if configure/build fails.

Current DSP supports mono-in/mono-out models only. Stereo models, models with multiple inputs/outputs, model-specific sample-rate conversion, latency reporting, and model swaps while audio is actively processing require additional testing. The shared pointer handoff is a prototype; for a hardened real-time plug-in, use a lock-free or audio-safe handoff and defer destruction of old model instances away from the audio thread. The current audio callback copies a dry buffer, which may allocate; replace with preallocated scratch buffers before production.

Mix is implemented as a simple dry/wet blend; if the NAM model's expected input/output calibration differs from the source audio, adjust input/output gain. A model loading error is shown in the status line.

## Dependencies

- Visual Studio 2022 with Desktop development with C++
- CMake 3.22+
- Git
- Internet access for CMake FetchContent
- A VST3-compatible DAW for testing

## Build on Windows (PowerShell)

From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Expected output is typically:

`build\NAMAudioProcessor_artefacts\Release\VST3\NAM Audio Processor.vst3`

Search if needed:

```powershell
Get-ChildItem .\build -Recurse -Filter "*.vst3"
```

If configure fails, look for the first CMake error. Common causes:
- `JuceHeader.h` missing: confirm `juce_generate_juce_header(NAMAudioProcessor)` is present after `juce_add_plugin(...)` and rerun the workflow.
- Missing Visual Studio C++ workload or Windows SDK
- CMake not in PATH
- Network/proxy blocks GitHub fetch
- Upstream NAM Core target or dependencies changed from the pinned tag

## Test

1. Copy the `.vst3` bundle to `C:\Program Files\Common Files\VST3`.
2. Rescan plug-ins in your DAW.
3. Insert NAM Audio Processor on a mono audio track.
4. Choose a legal `.nam` model file.
5. Confirm status changes to `Loaded: ...` and `MODEL ACTIVE`.
6. Play audio; compare bypass vs active model and adjust input/output gain.
7. Test clip indicator, dry/wet mix, and DAW save/reopen.
8. Test at 44.1 kHz and 48 kHz first; then test 88.2/96 kHz and different block sizes.
9. Test with models from different architectures; this prototype only accepts mono-in/mono-out.

## GitHub publish

Create an empty repository at https://github.com/new, then:

```powershell
git init
git add .
git commit -m "Integrate NeuralAmpModelerCore into NAM Audio Processor"
git branch -M main
git remote add origin https://github.com/YOUR-USERNAME/NAM-Audio-Processor.git
git push -u origin main
```

Replace `YOUR-USERNAME`. Do not commit `.nam` models unless the licence allows redistribution.

## Presets

Host state saves parameters and the absolute `.nam` file path. A portable preset browser / model copy / relative-path relocation is not yet implemented. When sharing sessions with another computer, the recipient must have the model at the same path or choose it again.

## Upstream references

- NAM Core loader API: https://neuralampmodelercore.readthedocs.io/en/latest/api/get_dsp.html
- NAM Core DSP API: https://neuralampmodelercore.readthedocs.io/en/latest/api/dsp.html
- NAM Core repository: https://github.com/sdatkinson/NeuralAmpModelerCore
- JUCE plugin framework: https://github.com/juce-framework/JUCE

## GitHub Actions CI build

This repository includes `.github/workflows/build-windows.yml`. After pushing to `main` or `master`, GitHub Actions will configure and build the VST3/Standalone targets on a Windows 2022 x64 runner, then upload a ZIP artifact if a `.vst3` bundle is found.

To run it manually, open the repository's **Actions** tab, select **Build NAM Audio Processor (Windows)**, and choose **Run workflow**. When the job succeeds, open that workflow run and download `NAM-Audio-Processor-Windows-VST3` under **Artifacts**. If the build fails, inspect the failed step's log; the workflow also attempts to upload available CMake logs.

A successful CI build only confirms that the source compiled and a VST3 bundle was packaged. It does not replace testing the plug-in in a DAW with a suitable `.nam` model.


## Debugging GitHub Actions build failures

If the Windows build fails, open the failed `Build VST3 on Windows x64` step and search the log for `error C` or `fatal error`. The workflow also uploads a `NAM-Audio-Processor-Windows-Build-Logs` artifact containing `build-windows.log` when a step fails. The final `Process completed with exit code 1` line is only the summary; the actionable compiler error is normally earlier in the log.


## Build reliability notes

- The NAM Core DSP channel API is `NumInputChannels()` / `NumOutputChannels()`. The project also defines `NAM_SAMPLE_FLOAT=1` publicly on the `namcore` target so the NAM Core `process()` sample type matches JUCE's `AudioBuffer<float>` on both sides of the API.
- The CMake configure fetches JUCE 8.0.6 and NAM Core v0.6.0 and requests recursive Git submodules for NAM Core dependencies.
- `juce_generate_juce_header(NAMAudioProcessor)` is called after JUCE modules are linked.
- GitHub Actions uploads `NAM-Audio-Processor-Windows-VST3` on success and a `NAM-Audio-Processor-Windows-Diagnostics` artifact on every run.
- This project has not been verified by a successful build on your GitHub runner in this session. If CI still fails, download the diagnostics artifact and inspect `build.log`; no source project can honestly guarantee zero errors on an untested external runner.
