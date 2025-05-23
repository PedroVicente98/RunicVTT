cmake_minimum_required(VERSION 3.20)
project(RunicVTT VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)

# ----------------------
# Sources
# ----------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/network/*.cpp
    src/renderer/*.cpp
    vendor/imgui/*.cpp
)

list(REMOVE_ITEM SOURCES
    ${CMAKE_SOURCE_DIR}/src/Application_OLD.cpp # REMOVE AT RELEASE :) I MIGHT FORGOR
    ${CMAKE_SOURCE_DIR}/src/NetworkManager_OLD.cpp # REMOVE AT RELEASE :) I MIGHT FORGOR
    ${CMAKE_SOURCE_DIR}/include/NetworkManager_OLD.h
)


add_executable(RunicVTT ${SOURCES})

# ----------------------
# Include Directories
# ----------------------
target_include_directories(RunicVTT PRIVATE
    include
    include/renderer
    include/network
    dependencies/GLEW/include
    dependencies/OpenSSL/include
    vendor/imgui
    vendor/stb
    vendor/glm
    vendor/glfw/include
    vendor/flecs/include
)

# --- OpenSSL config (antes de add_subdirectory(libdatachannel)) ---
set(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/dependencies/OpenSSL" CACHE PATH "Path to OpenSSL root")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include" CACHE PATH "Path to OpenSSL include")
set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libcrypto.lib" CACHE FILEPATH "OpenSSL crypto lib")
set(OPENSSL_SSL_LIBRARY "${OPENSSL_ROOT_DIR}/lib/libssl.lib" CACHE FILEPATH "OpenSSL ssl lib")
set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "Use OpenSSL static libs")


# Link to RunicVTT (optional but fine)
target_link_libraries(RunicVTT PRIVATE
    "${OPENSSL_SSL_LIBRARY}"
    "${OPENSSL_CRYPTO_LIBRARY}"
)

# ----------------------
# libdatachannel (Static)
# ----------------------
set(DCHANNEL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/libdatachannel)
target_link_libraries(RunicVTT PRIVATE datachannel)

# ----------------------
# GLFW (Static)
# ----------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/glfw)
target_link_libraries(RunicVTT PRIVATE glfw)

# ----------------------
# GLEW (Static)
# ----------------------
target_compile_definitions(RunicVTT PRIVATE GLEW_STATIC)
target_link_libraries(RunicVTT PRIVATE ${CMAKE_SOURCE_DIR}/dependencies/GLEW/lib/Release/x64/glew32s.lib)
target_link_libraries(RunicVTT PRIVATE opengl32)

# ----------------------
# Flecs (Static)
# ----------------------
set(FLECS_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/flecs)
target_link_libraries(RunicVTT PRIVATE flecs::flecs_static)


# ----------------------
# Post Build: Copy OpenSSL DLLs
# ----------------------
add_custom_command(TARGET RunicVTT POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/dependencies/OpenSSL/bin/libssl-3-x64.dll"
        "$<TARGET_FILE_DIR:RunicVTT>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/dependencies/OpenSSL/bin/libcrypto-3-x64.dll"
        "$<TARGET_FILE_DIR:RunicVTT>"
)

# ----------------------
# Copy resource folder
# ----------------------
add_custom_command(TARGET RunicVTT POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:RunicVTT>/res"
)

## ----------------------
# CPACK Installer
# ----------------------
#include(InstallRequiredSystemLibraries)

#set(CPACK_PACKAGE_NAME "RunicVTT")
#set(CPACK_PACKAGE_VERSION "1.0.0")
#set(CPACK_PACKAGE_VENDOR "Runic")
#set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Runic Virtual Tabletop Application")
#set(CPACK_PACKAGE_INSTALL_DIRECTORY "RunicVTT")

#set(CPACK_GENERATOR "NSIS")

# Shortcut on desktop
#set(CPACK_NSIS_CREATE_ICONS_EXTRA
#  "CreateShortCut \\\"$DESKTOP\\\\RunicVTT.lnk\\\" \\\"$INSTDIR\\\\RunicVTT.exe\\\""
#)

# Install binary
#install(TARGETS RunicVTT DESTINATION .)

# Install resources
#install(DIRECTORY ${CMAKE_SOURCE_DIR}/res DESTINATION res)

# Install OpenSSL DLLs
#install(FILES
#    ${CMAKE_SOURCE_DIR}/dependencies/OpenSSL/bin/libssl-3-x64.dll
#    ${CMAKE_SOURCE_DIR}/dependencies/OpenSSL/bin/libcrypto-3-x64.dll
#    DESTINATION .
#)

#include(CPack)
