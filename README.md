[![Build & Package (Windows)](https://github.com/PedroVicente98/RunicVTT/actions/workflows/release-application.yml/badge.svg)](https://github.com/PedroVicente98/RunicVTT/actions/workflows/release-application.yml)
[![Build & Publish Doxygen](https://github.com/PedroVicente98/RunicVTT/actions/workflows/build_doxygen_and_publish.yml/badge.svg?branch=main)](https://github.com/PedroVicente98/RunicVTT/actions/workflows/build_doxygen_and_publish.yml)

# Runic VTT

**Runic VTT** is a peer-to-peer **Virtual Tabletop (VTT)** for tabletop RPGs, built in C++ with [libdatachannel](https://github.com/paullouisageneau/libdatachannel).  
It enables direct peer-to-peer sessions without the need for centralized servers.

---
## Documentation
The complete Doxygen Documentation can be found [here](https://pedrovicente98.github.io/runicvtt.github.io/)

> [!note]
> The documentation is updated when pushing to the *main* branch.

---
## Features
- Peer-to-peer networking with WebRTC (`libdatachannel`)
- Entity-Component System (ECS) with `flecs`
- Modern UI built on `ImGui`
- OpenGL-based rendering with `GLEW`, `GLFW`, and `GLM`

---

## Screenshots & Demos
![Load Game Table](https://i.imgur.com/6qJ7tvt.gif)  
![Docking Layout](https://i.imgur.com/cICVLM6.gif)  
![Edit Marker](https://i.imgur.com/EkGODX9.gif)  

More examples in the [gallery](https://imgur.com/a/pXZvuC5).

---

## Dependencies
The project uses the following libraries:

| Library | Purpose |
|---------|---------|
| [ImGui](https://github.com/ocornut/imgui) | UI system & Input Handling |
| [Flecs](https://github.com/SanderMertens/flecs) | Entity-Component System |
| [GLEW](https://github.com/nigels-com/glew) | OpenGL Extension Loader |
| [GLFW](https://github.com/glfw/glfw) | Windowing & Input Handling |
| [libdatachannel](https://github.com/paullouisageneau/libdatachannel) | WebRTC-based P2P Networking |
| [stb_image](https://github.com/nothings/stb) | Image loading |
| [GLM](https://github.com/g-truc/glm) | Math library |
| [Localtunnel](localtunnel.me) | Tunneling for signal websocket server (No Port Fowarding Needed) |
| [Node.js](https://nodejs.org/pt) | Portable Node for running Localtunnel |
| [nlohmann json](https://github.com/nlohmann/json) | JSON and Messaging Formatting |
---

## Build Instructions

### Prerequisites
- Windows 10/11  
- C++20 compatible compiler  
- [CMake](https://cmake.org/) ≥ 3.16  
- Visual Studio 2022 (recommended)  

### Clone & Build
```bash
git clone https://github.com/PedroVicente98/RunicVTT.git
cd RunicVTT
cmake -S . -B build
cmake --build build --config Release

# The resulting binaries will be located in build/Release
```

## Releases & Downloads
- Stable builds are available on the [Releases page](https://github.com/PedroVicente98/RunicVTT/releases).  
- Each release includes:
  - A portable application zip (Executable on the bin/ folder)
  - A source code zip 

---

## Reporting Bugs
If you encounter a bug:
1. Check the [issue tracker](https://github.com/PedroVicente98/RunicVTT/issues) to see if it has already been reported.  
2. Open a new issue using the **Bug Report** template.  
   - Please include logs, screenshots, or GIFs if possible.  

---

## Contributing
Contributions are welcome!  
- Open issues or feature requests in the tracker.  
- Fork the repository and submit a Pull Request.  
- Follow existing coding style and include documentation/comments where appropriate.
---

## Roadmap / To do 
- Expand Markdown Notes Editor Functionality.  
- No Grid Distance Measures.  
- Hexagonal Grid.  
- Entity Manager Window.
- UI Overhaul
- Portuguese Localization
- Add Emojis
- More..

---

## License
This project is licensed under the [MIT License](LICENSE).

