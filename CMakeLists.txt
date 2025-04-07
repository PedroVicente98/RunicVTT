cmake_minimum_required(VERSION 3.20)
project(MyApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ----------------------
# Executable
# ----------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    vendor/imgui/*.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(MyApp ${SOURCES})

target_include_directories(MyApp PRIVATE
    include
    vendor/glew/include
    vendor/openssl/include
    vendor/imgui
    vendor/imgui/backends
    vendor/stb
    vendor/glm
)

# ----------------------
# Static Libraries
# ----------------------

# GLEW (Static)
target_link_libraries(MyApp PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/glew/lib/glew32s.lib
)
target_compile_definitions(MyApp PRIVATE GLEW_STATIC)

# Flecs (Static)
add_subdirectory(vendor/flecs)
target_link_libraries(MyApp PRIVATE flecs::flecs)

# libdatachannel (Static, but uses OpenSSL)
add_subdirectory(vendor/libdatachannel)
target_link_libraries(MyApp PRIVATE datachannel)

# stb + glm are headers only (already included above)

# ----------------------
# OpenSSL (Dynamic)
# ----------------------
target_link_libraries(MyApp PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/openssl/lib/libssl.lib
    ${CMAKE_SOURCE_DIR}/vendor/openssl/lib/libcrypto.lib
)

# ----------------------
# Post Build: Copy OpenSSL DLLs
# ----------------------
add_custom_command(TARGET MyApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/vendor/openssl/DLLs/libssl-3-x64.dll"
        "$<TARGET_FILE_DIR:MyApp>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/vendor/openssl/DLLs/libcrypto-3-x64.dll"
        "$<TARGET_FILE_DIR:MyApp>"
)

# ----------------------
# Copy resource folder
# ----------------------
add_custom_command(TARGET MyApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:MyApp>/res"
)



# ==== CPACK ====
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "MyApp")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "MyCompany")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "My WebRTC + OpenGL Application")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "MyApp")

set(CPACK_GENERATOR "NSIS")

# Shortcut on desktop
set(CPACK_NSIS_CREATE_ICONS_EXTRA
  "CreateShortCut \\\"$DESKTOP\\\\MyApp.lnk\\\" \\\"$INSTDIR\\\\MyApp.exe\\\""
)

# Install main binary
install(TARGETS MyApp DESTINATION .)

# Install resource folder
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res DESTINATION res)

# Only OpenSSL DLLs (because others are static)
install(FILES
    ${CMAKE_SOURCE_DIR}/vendor/openssl/DLLs/libssl-3-x64.dll
    ${CMAKE_SOURCE_DIR}/vendor/openssl/DLLs/libcrypto-3-x64.dll
    DESTINATION .
)

include(CPack)

