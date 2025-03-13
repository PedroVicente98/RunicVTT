# Runic VTT

🚀 A Peer-to-Peer **Virtual Tabletop (VTT)** for TTRPGs, built using C++ and `libdatachannel`.  
No servers needed – just direct **peer-to-peer connectivity**! 🎲✨

## 🌟 Features
- ✅ **Peer-to-Peer Networking** – No centralized server, powered by `libdatachannel`.
- ✅ **Fast & Lightweight** – Runs efficiently with low-latency communication.
- ✅ **Modern UI with ImGui** – A sleek, customizable interface.
- ✅ **ECS-based Architecture** – Using `flecs` for efficient entity management.
- ✅ **Cross-Platform** – Works on Windows, Linux, and macOS.
- ✅ **Open Source** – Free to use and modify!

---

## 📷 Screenshots
![LoadGameTable](https://imgur.com/6qJ7tvt.gif)

![Docking](https://imgur.com/cICVLM6.gif)

![EditMarker](https://imgur.com/EkGODX9.gif)

Click [here](https://imgur.com/a/pXZvuC5) for the screenshot galery.
---

## 📦 Dependencies
Runic VTT uses the following libraries:

| Library        | Purpose |
|---------------|---------|
| **[ImGui](https://github.com/ocornut/imgui)** | UI system |
| **[Flecs](https://github.com/SanderMertens/flecs)** | Entity-Component System |
| **[GLEW](https://github.com/nigels-com/glew)** | OpenGL Extension Loader |
| **[GLFW](https://github.com/glfw/glfw)** | Windowing & Input Handling |
| **[libdatachannel](https://github.com/paullouisageneau/libdatachannel)** | WebRTC-based P2P Networking |
| **[stb_image](https://github.com/nothings/stb)** | Image Loading |
| **[GLM](https://github.com/g-truc/glm)** | Mathematics Library |

*(Runic VTT is fully open-source and relies on these excellent libraries!)*

---

## 🛠️ Building Runic VTT

### 🔹 **Prerequisites**
- **C++17** or later
- **CMake** (>= 3.16)
- **OpenSSL** (for `libdatachannel`)
- **A C++ Compiler**:
  - **Windows**: MSVC (Visual Studio 2019+ recommended)
  - **Linux/macOS**: GCC or Clang

### 🔹 **Clone the Repository**
```bash
git clone https://github.com/yourusername/runic-vtt.git
cd runic-vtt
