---
uuid: d47a1bd6c24e2b7a79aab702e95ef4a8
title: README
author: unknown
creation_ts: 1761000987577
last_update_ts: 1761001012809
table: 
shared: 0
shared_from: 
---
# Runic VTT

**Runic VTT** is a peer-to-peer **Virtual Tabletop (VTT)** for tabletop RPGs, built in C++ with [libdatachannel](https://github.com/paullouisageneau/libdatachannel).  
It enables direct peer-to-peer sessions without the need for centralized servers.

---

## Features
- Peer-to-peer networking with WebRTC (`libdatachannel`)
- Entity-Component System (ECS) with `flecs`
- Modern UI built on `ImGui`
- OpenGL-based rendering with `GLEW`, `GLFW`, and `GLM`
- Cross-platform architecture (primary target: Windows)

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
- [OpenSSL](https://www.openssl.org/) (required by `libdatachannel`)  
- Visual Studio 2022 (recommended)  

### Clone & Build
```bash
git clone https://github.com/PedroVicente98/RunicVTT.git
cd RunicVTT
cmake -S . -B build
cmake --build build --config Release

# The resulting binaries will be located in build/Release
```

## Usage
- Launch the application from the build folder.  
- Create or join a session by exchanging peer connection details.  
- Use the UI to load maps, place tokens, and manage game state.  

*(A step-by-step usage guide with screenshots will go here TODO.)*

---

## Releases & Downloads
- Stable builds are available on the [Releases page](https://github.com/PedroVicente98/RunicVTT/releases).  
- Each release includes:
  - Windows executable installer   
  - A portable application folder (Installer contents already configured)
  - A source code zip (tagged version)

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

## Roadmap / How it Works
- Markdown Notes Editor.  
- No Grid Distance Measures.  
- Hexagonal Grid.  
- Entity Manager Window.  
---

## License
This project is licensed under the [MIT License](LICENSE).

