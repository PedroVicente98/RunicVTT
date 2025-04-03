cmake_minimum_required(VERSION 3.16)
project(RunicVTT)

# Use C++17
set(CMAKE_CXX_STANDARD 17)

# Define the source and include directories
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# Create the executable
file(GLOB_RECURSE SRC_FILES "${SRC_DIR}/*.cpp")
add_executable(runic_vtt ${SRC_FILES})

# Include application headers
target_include_directories(runic_vtt PRIVATE ${INCLUDE_DIR})



# Add ImGui as a Static Library
add_library(imgui STATIC 
    vendor/imgui/imgui.cpp 
    vendor/imgui/imgui_demo.cpp 
    vendor/imgui/imgui_draw.cpp 
    vendor/imgui/imgui_tables.cpp 
    vendor/imgui/imgui_widgets.cpp 
    vendor/imgui/backends/imgui_impl_glfw.cpp 
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

# Include ImGui headers
target_include_directories(imgui PUBLIC vendor/imgui vendor/imgui/backends)

# Link ImGui with OpenGL & GLFW (needed for backends)
target_link_libraries(imgui PRIVATE glfw OpenGL::GL)


# Add Flecs as a Static Library
add_subdirectory(vendor/flecs)
target_link_libraries(runic_vtt PRIVATE flecs)


#set(USE_GNUTLS ON CACHE BOOL "Use GNUTLS for DTLS")
#set(USE_SRTP OFF CACHE BOOL "Disable SRTP since we're not using RTP")
#set(USE_SCTP ON CACHE BOOL "Use usrsctp for DataChannels")

# Add libdatachannel (Dynamic)
#add_subdirectory(vendor/libdatachannel)
#target_link_libraries(runic_vtt PRIVATE datachannel)


# Add GLFW (Dynamic)
add_subdirectory(vendor/glfw)
target_link_libraries(runic_vtt PRIVATE glfw OpenGL::GL)

# Add GLEW (Dynamic)
add_subdirectory(vendor/glew/build/cmake)
target_link_libraries(runic_vtt PRIVATE glew)
target_include_directories(runic_vtt PRIVATE vendor/glew/include)


# Include Header-Only Libraries
target_include_directories(runic_vtt PRIVATE vendor/glm vendor/stb)


# Link Everything to runic_vtt
target_link_libraries(runic_vtt PRIVATE 
    imgui         # GUI
    flecs         # ECS System
    glfw glew     # Graphics
   # datachannel   # WebRTC (Networking)
)
