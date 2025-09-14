# wtop (Windows Minimal Performance Overlay)

A tiny, always-on-top, click-through performance overlay for Windows 10/11 showing CPU, memory, network, and disk activity as minimalist sparklines.

## Features (Current MVP)
- Borderless transparent overlay (color key) you can place anywhere.
- Click-through by default so it doesn't block interaction.
- Toggle lock (click-through) with tray menu or hotkey `Ctrl+Shift+O`.
- CPU, Memory, Network (In/Out), Disk (Read/Write) rolling sparklines.
- System tray icon with quick menu (Lock/Unlock, Exit).

## Planned / Ideas
- Config file for colors, scaling, position persistence.
- Per-pixel alpha & Direct2D rendering for smoother lines.
- GPU utilization.
- Adjustable sampling interval.

## Build Instructions

### Prerequisites
- Windows 10 or 11
- CMake >= 3.20
- A C++17 compiler (MSVC recommended; install via Visual Studio or Build Tools).

### Steps
```pwsh
# From repository root
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target wtop
# Binary will be in build/bin (or build/Release/bin depending on generator)
```

If using the Visual Studio generator you can also open the generated solution in `build/`.

## Usage
Run the produced `wtop.exe`. You'll see a small transparent overlay.

Controls:
- `Ctrl+Shift+O`: Toggle click-through (unlock to drag the window).
- Right-click tray icon: Lock/Unlock or Exit.

## Notes
- Network & Disk graphs are normalized to fixed scales (100 MB/s net, 400 MB/s disk) for now.
- First few seconds may show empty graphs while baseline samples accumulate.
- CPU usage derived via `GetSystemTimes` deltas.
- Disk stats via PDH `PhysicalDisk(_Total)` counters.

## License
MIT (add a LICENSE file if distributing publicly).
