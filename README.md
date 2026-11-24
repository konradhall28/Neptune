# Neptune MB

Multiband distortion/saturation plug-in (VST3 + Standalone) in the style of FabFilter Saturn 2, with a blue UI. JUCE 8, C++17.
Independent project, not affiliated with FabFilter.

## Build on GitHub
Upload the contents of this folder (including the hidden `.github` folder) to a GitHub repo. Actions builds the
Windows VST3. Download the `NeptuneMB-Windows-VST3` artifact and copy `Neptune MB.vst3` to
`C:\Program Files\Common Files\VST3`, then rescan in Studio Pro.

## Build locally
    git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release --target NeptuneMB_VST3

## Accuracy
Styles are fitted to published black-box measurements of Saturn 2.1.3 (see `fit/`). Configure with
`-DCPP_BUILD_SNAPSHOT=ON` and run `UISnapshot <dir>` to render screenshots and print the per-style self-test.
Edit `Source/StyleTable.h` (or rerun `fit/gen_table.py`) to tune a style.
