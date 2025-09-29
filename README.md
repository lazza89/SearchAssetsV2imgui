# SearchAssets ImGui

A modern, cross-platform GUI application for searching assets in Unreal Engine projects, built with ImGui and OpenGL.

![SearchAssets ImGui Interface](https://via.placeholder.com/800x600/2c3e50/ffffff?text=SearchAssets+ImGui)

## Features

✅ **Modern GUI Interface** - ImGui-based responsive UI
✅ **Perfect Windows 11 Compatibility** - No terminal size conflicts
✅ **Windowed Locked Mode** - Fixed size window, cannot be moved or resized
✅ **Real-time Search Results** - Live table updates
✅ **Advanced Filtering** - Filter results as you type
✅ **Copy to Clipboard** - Individual or bulk copy operations
✅ **Multithreaded Performance** - Same high-performance backend as terminal version
✅ **Unreal Prefix Removal** - Automatic A/U/F/S/T/E/I prefix handling
✅ **Plugin Search Support** - Search through plugin Content directories
✅ **Locked Interface** - Window cannot be moved, resized, or minimized

## Advantages over Terminal Version

- **No Windows 11 compatibility issues**
- **Better resize handling**
- **Professional table-based results display**
- **Improved clipboard operations**
- **More responsive interface**
- **Native window controls**

## Build Requirements

- **CMake 3.20+**
- **C++20 compatible compiler**
- **OpenGL 3.3+** support
- **Git** (for dependency fetching)

## Building

### Windows (Visual Studio)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Linux/macOS
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Usage

1. **Launch**: Run `SearchAssetsImGui.exe`
2. **Search Pattern**: Enter your search term (regex supported)
3. **Start Search**: Click "Start Search" or press F5
4. **Filter Results**: Type in the filter box to narrow results
5. **Copy Results**: Click items to select, then "Copy Selected"

### Keyboard Shortcuts

- **F5**: Start search
- **Escape**: Stop search (when searching)
- **Double-click result**: Copy to clipboard

### Exit Methods

1. **Window Close Button**: Click the X button on the window
2. **Exit Menu**: Use the "Exit" → "Quit Application" menu option
3. **Alt+F4**: Standard Windows close shortcut

## Architecture

- **SearchEngine.cpp/h**: Multithreaded search engine (shared with terminal version)
- **SearchAssetsGUI.cpp/h**: ImGui interface implementation
- **main.cpp**: OpenGL/GLFW setup and main loop

## Dependencies (Auto-downloaded)

- **GLFW**: Window management
- **ImGui**: Immediate mode GUI
- **OpenGL**: Graphics rendering

## Project Structure

```
SearchAssets ImGui/
├── src/
│   ├── SearchEngine.h/cpp     # Core search logic
│   ├── SearchAssetsGUI.h/cpp  # ImGui interface
│   └── main.cpp               # Application entry point
├── build/                     # Build output
├── CMakeLists.txt            # Build configuration
└── README.md                 # This file
```

## Performance

- **Same multithreaded engine** as terminal version
- **Memory-mapped file I/O** for optimal performance
- **Lock-free result updates** during search
- **GPU-accelerated rendering** via OpenGL

## License

This project maintains the same architecture and search engine as the original terminal version, with a modern ImGui frontend for improved usability and Windows 11 compatibility.