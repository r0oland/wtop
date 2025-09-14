# wtop - Windows Performance Overlay

A super minimal, transparent performance overlay for Windows 10/11 that displays real-time system metrics with sparkline graphs.

## Features

- **Real-time Performance Graphs**: CPU, Memory, and Network utilization sparklines
- **Always Visible**: Transparent overlay that stays on top of all windows
- **Click-through Toggle**: Right-click to enable/disable mouse interaction
- **Smart Positioning**: Auto-docks near taskbar clock or manual positioning
- **Network Interface Selection**: Choose specific network adapter via right-click menu
- **Dynamic Units**: Automatic B/s → KB/s → MB/s → GB/s formatting
- **Minimal Resource Usage**: Lightweight C++ implementation using Win32 APIs
- **Tray Integration**: System tray icon with context menu
- **Hotkey Support**: Ctrl+Shift+O to toggle visibility

## Metrics Displayed

- **CPU Usage**: Percentage and sparkline graph
- **Memory Usage**: Percentage and sparkline graph  
- **Network Utilization**: Percentage of interface capacity + throughput rates
- **Disk I/O**: Read/write throughput with dynamic units

## Quick Start

### Build from Source
```bash
# Clone the repository
git clone https://github.com/r0oland/wtop.git
cd wtop

# Build with CMake (requires Visual Studio Build Tools)
cmake -S . -B build
cmake --build build --config Release

# Run
./build/bin/Release/wtop.exe
```

## Usage

### Controls
- **Right-click** on overlay: Open context menu
- **Left-click + drag**: Move overlay (disables auto-docking)
- **Ctrl+Shift+O**: Toggle visibility
- **Right-click tray icon**: Same context menu

### Context Menu Options
- **Enable/Disable Click-through**: Toggle mouse interaction
- **Auto-dock/Manual Position**: Toggle automatic positioning near taskbar
- **Network Interface**: Select specific network adapter or auto-select fastest
- **Exit**: Close application

### Auto-Docking Behavior
- Automatically positions near taskbar clock
- Supports all taskbar orientations (bottom, top, left, right)
- Disabled after manual drag - re-enable via context menu

## Requirements

- Windows 10/11
- Visual C++ Redistributable (for pre-built binaries)

## Build Requirements

- CMake 3.20+
- Visual Studio 2019+ or Build Tools
- Windows SDK

## Architecture

- **Language**: C++17
- **APIs**: Win32, PDH (Performance Data Helper), IP Helper API
- **Graphics**: GDI for lightweight rendering
- **Build System**: CMake with MSVC

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

MIT License - see [LICENSE](LICENSE) file for details.
