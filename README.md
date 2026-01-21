# Clarity Layer

Windows accessibility application for low-vision/near-blind users providing GPU-based visual enhancement, magnification, and reading assistance.

## Features

### Visual Enhancement Layer
- GPU-accelerated screen transforms using DirectX 11 shaders
- Contrast, brightness, gamma, and saturation adjustments
- Color inversion (full or brightness-only/hue-preserving)
- Edge enhancement for improved visibility
- Click-through overlay that works across all applications

### Magnification
- Full-screen zoom with smooth panning
- Lens mode (magnified region follows cursor)
- Multiple follow modes: cursor, caret, keyboard focus
- Uses Windows Magnification API when available

### Reading Assist
- UI Automation integration for reading accessible content
- SAPI text-to-speech for focused elements and selections
- OCR fallback for non-accessible content (images, PDFs)
- Speak under cursor (Win+S)

### Robust Control
- Global hotkeys that work system-wide
- Panic-off (Ctrl+Alt+X) - instantly disables all effects
- Safe mode (hold Shift during startup)
- Audio feedback for all state changes
- Profile system for saving/loading configurations

## Requirements

- Windows 10 version 1903 (May 2019 Update) or later
- DirectX 11 compatible GPU
- Visual Studio 2019 or later with C++20 support
- Windows SDK 10.0.18362.0 or later
- vcpkg package manager

## Building

### Prerequisites

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with:
   - Desktop development with C++
   - Windows 10/11 SDK
   - (Optional) C++ Clang tools for Windows

2. Install [vcpkg](https://vcpkg.io/):
   ```cmd
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   bootstrap-vcpkg.bat
   vcpkg integrate install
   ```

3. Set environment variable:
   ```cmd
   set VCPKG_ROOT=C:\path\to\vcpkg
   ```

4. Install dependencies:
   ```cmd
   vcpkg install nlohmann-json:x64-windows spdlog:x64-windows
   ```

5. Install [Ninja](https://ninja-build.org/) (recommended for faster builds):
   ```cmd
   winget install Ninja-build.Ninja
   ```

### Build with CMake Presets (Recommended)

The project includes CMake presets for easy configuration:

```cmd
# List available presets
cmake --list-presets

# Build with MSVC (Release)
cmake --preset msvc-release
cmake --build --preset msvc-release

# Build with Clang (Release)
cmake --preset clang-release
cmake --build --preset clang-release
```

### Build with Visual Studio

```cmd
# Generate Visual Studio solution
cmake --preset vs2022

# Or with Clang toolset
cmake --preset vs2022-clang
```

Then open `build/vs2022/ClarityLayer.sln` in Visual Studio.

### Manual CMake Build

```cmd
mkdir build && cd build

# MSVC
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Clang
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl

cmake --build .
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | OFF | Build unit tests |
| `ENABLE_ASAN` | OFF | Enable Address Sanitizer (Clang only) |

## Usage

### Hotkeys

| Hotkey | Action |
|--------|--------|
| Win+E | Toggle visual enhancement |
| Win+M | Toggle magnifier |
| Win+Plus | Zoom in |
| Win+Minus | Zoom out |
| Win+F | Speak focused element |
| Win+S | Speak text under cursor |
| Escape | Stop speaking |
| Ctrl+Alt+X | **PANIC OFF** - disable all effects |
| Win+1/2/3 | Quick switch profiles |
| Win+L | Toggle lens mode |
| Win+T | Cycle follow mode (cursor/caret/focus) |

### Safe Mode

Hold **Shift** while starting Clarity Layer to enter safe mode. All visual effects will be disabled, allowing you to configure settings safely.

### Profiles

Profiles are stored as JSON files in the `profiles` directory:
- `profiles/default.json` - Default profile
- User profiles are stored in `%LOCALAPPDATA%\ClarityLayer\profiles`

## Architecture

```
src/
├── main.cpp                    # Entry point, message loop
├── core/
│   ├── Controller              # Central coordinator
│   ├── ProfileManager          # Profile load/save/switch
│   └── HotkeyService           # Global hotkey registration
├── overlay/
│   ├── CaptureManager          # Windows.Graphics.Capture
│   ├── OverlayWindow           # Click-through D3D11 window
│   ├── ShaderPipeline          # Transform shaders
│   └── shaders/                # HLSL shaders
├── magnifier/
│   ├── MagnifierController     # Zoom state management
│   ├── FocusTracker            # Cursor/caret/focus tracking
│   └── LensRenderer            # Lens mode rendering
├── reader/
│   ├── AccessibilityReader     # UI Automation wrapper
│   ├── OcrReader               # Windows.Media.Ocr wrapper
│   └── SpeechEngine            # SAPI ISpVoice wrapper
├── ui/
│   ├── SettingsWindow          # Main settings UI
│   └── TrayIcon                # System tray
└── util/
    ├── Logger                  # Logging system
    ├── AudioFeedback           # Mode change sounds
    └── SafeMode                # Panic-off, recovery
```

## Safety Features

- **Panic Off**: Ctrl+Alt+X instantly disables all visual effects
- **Safe Mode**: Hold Shift during startup to disable all effects
- **Watchdog**: Auto-recovery if application becomes unresponsive
- **Click-through**: Overlay never blocks mouse/keyboard input
- **Self-capture prevention**: Overlay excludes itself from capture to avoid feedback loops

## Technical Notes

### Overlay Approach

Based on [ShaderGlass](https://github.com/mausimus/ShaderGlass):
- Uses Windows.Graphics.Capture API for desktop capture
- DirectX 11 for GPU-accelerated shader processing
- WS_EX_LAYERED | WS_EX_TRANSPARENT for click-through overlay
- SetWindowDisplayAffinity to exclude from capture

### Multi-Monitor Support

- Captures and overlays all monitors
- Per-monitor DPI awareness (v2)
- Handles dynamic monitor connect/disconnect

### OCR

Uses Windows.Media.Ocr for recognizing text in:
- Images without alt text
- PDFs rendered as images
- Applications without accessibility support

## License

[License TBD]

## Acknowledgments

- [ShaderGlass](https://github.com/mausimus/ShaderGlass) - GPU overlay reference
- Windows Magnification API documentation
- UI Automation documentation
