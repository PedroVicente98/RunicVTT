cmake_minimum_required(VERSION 3.20)

find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX REPLACE "^v" "" GIT_TAG "${GIT_TAG}")

if(GIT_TAG MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    set(PROJECT_VERSION ${GIT_TAG})
else()
    set(PROJECT_VERSION "0.1.0")
endif()

project(RunicVTT VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "" FORCE)

# ----------------------
# Sources
# ----------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/network/*.cpp
    src/note_editor/*.cpp
    src/note_editor/*.c
    src/renderer/*.cpp
    vendor/imgui/*.cpp
)

add_executable(RunicVTT ${SOURCES})
# Always build without console window on Windows:
if (WIN32)
  if (MSVC)
    # GUI subsystem, but still call main() as entry
    target_link_options(RunicVTT PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  elseif (MINGW)
    # MinGW/GCC equivalent
    target_link_options(RunicVTT PRIVATE -mwindows)
  endif()
endif()

# ----------------------
# Include Directories
# ----------------------
target_include_directories(RunicVTT PRIVATE
    include
    include/renderer
    include/network
    include/note_editor
    include/debug
    dependencies/GLEW/include
    dependencies/OpenSSL/include
    vendor/imgui
    vendor/stb
    vendor/glm
    vendor/glfw/include
    vendor/flecs/include
    vendor/json/single_include
    vendor/libdatachannel/include
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
target_link_libraries(RunicVTT PRIVATE winhttp ws2_32 ) #Shell32 Ole32
# ----------------------
# nlohhmann json
# ----------------------
#add_subdirectory(vendor/json)
#target_link_libraries(RunicVTT PRIVATE nlohmann_json)# ----------------------



# ----------------------
# Global/Precise Test and Example Disabling Flags
# This block sets common and specific options BEFORE adding subdirectories.
# This ensures subprojects configure themselves without building unwanted tests/examples.

# Global test disabling (kept as requested for general reassurance)
set(BUILD_TESTING OFF CACHE BOOL "Disable all CMake tests globally" FORCE)

# Precise disabling for libdatachannel and its submodules, based on its options
set(LIBSRTP_TEST_APPS OFF CACHE BOOL "Disable libsrtp tests" FORCE) # Specific for libsrtp
set(TEST_APPS OFF CACHE BOOL "Disable usrsctp tests" FORCE)      # Specific for usrsctp
set(JUICE_BUILD_TESTS OFF CACHE BOOL "Disable libjuice tests" FORCE) # Specific for libjuice

# Explicitly use libdatachannel's NO_EXAMPLES and NO_TESTS flags
set(NO_EXAMPLES ON CACHE BOOL "Disable libdatachannel examples" FORCE)
set(NO_TESTS ON CACHE BOOL "Disable libdatachannel tests build" FORCE)

# Precise disabling for GLFW, based on its options
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Disable GLFW documentation" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Disable GLFW test programs" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Disable GLFW example programs" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Disable GLFW installation target" FORCE) # To prevent install rules

# Precise disabling for Flecs, based on its options
set(FLECS_TESTS OFF CACHE BOOL "Build flecs tests" FORCE)
set(FLECS_SHARED OFF CACHE BOOL "Disable building shared Flecs library" FORCE) # To ensure static linking

# ----------------------
# libdatachannel (Static)
add_subdirectory(vendor/libdatachannel)
target_link_libraries(RunicVTT PRIVATE datachannel)

# ----------------------
# GLFW (Static)
# ----------------------

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
# Copy miniupnpc folder
# ----------------------
add_custom_command(TARGET RunicVTT POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/dependencies/miniupnpc/upnpc-static.exe"
        "$<TARGET_FILE_DIR:RunicVTT>"
)

# ----------------------
# Copy external folder
# ----------------------
add_custom_command(TARGET RunicVTT POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/external"
        "$<TARGET_FILE_DIR:RunicVTT>/external"
)

# ----------------------
# Copy resource folder
# ----------------------
add_custom_command(TARGET RunicVTT POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:RunicVTT>/res"
)

# ----------------------
# Install Rules
# ----------------------
# Install the main executable (to Program Files/RunicVTT)
install(TARGETS RunicVTT
    RUNTIME DESTINATION .
)

# Install resources (if you have folders like res/ and external/)
install(DIRECTORY res/ DESTINATION res)
install(DIRECTORY external/ DESTINATION external)

# ----------------------
# CPack Configuration
# ----------------------
include(InstallRequiredSystemLibraries)

# Package metadata
set(CPACK_PACKAGE_NAME "RunicVTT")
set(CPACK_PACKAGE_VENDOR "Pedro Vicente dos Santos")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RunicVTT - Virtual Tabletop")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "pedrovicente45@outlook.com")

# Default install directory (Program Files on Windows)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "RunicVTT")

# Generators: create both ZIP and NSIS installer
set(CPACK_GENERATOR "ZIP;NSIS")

# NSIS-specific settings
set(CPACK_NSIS_DISPLAY_NAME "RunicVTT")
set(CPACK_NSIS_HELP_LINK "https://github.com/PedroVicente98/RunicVTT")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/PedroVicente98")
set(CPACK_NSIS_CONTACT "pedrovicente45@outlook.com")

# Add uninstaller entry in Control Panel
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

# Create Start Menu shortcut
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\RunicVTT.lnk\\\" \\\"$INSTDIR\\\\RunicVTT.exe\\\""
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\RunicVTT.lnk\\\""
)

# Include CPack (must be last!)
include(CPack)




## ----------------------
# CPACK Installer
# ----------------------
# in clude(InstallRequiredSystemLibraries)

#s et(CPACK_PACKAGE_NAME "RunicVTT")
#se t(CPACK_PACKAGE_VERSION "1.0.0")
#s et(CPACK_PACKAGE_VENDOR "Runic")
#se t(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Runic Virtual Tabletop Application")
#se t(CPACK_PACKAGE_INSTALL_DIRECTORY "RunicVTT")

#s et(CPACK_GENERATOR "NSIS")

# Shortcut on desktop
#s et(CPACK_NSIS_CREATE_ICONS_EXTRA
#  "CreateShortCut \\\"$DESKTOP\\\\RunicVTT.lnk\\\" \\\"$INSTDIR\\\\RunicVTT.exe\\\""
#)

# Install binary
#in stall(TARGETS RunicVTT DESTINATION .)

# Install resources
# in stall(DIRECTORY ${CMAKE_SOURCE_DIR}/res DESTINATION res)
#ins tall(DIRECTORY ${CMAKE_SOURCE_DIR}/external DESTINATION external)

# Install OpenSSL DLLs
# i nstall(FILES
 #   ${CMAKE_SOURCE_DIR}/dependencies/OpenSSL/bin/libssl-3-x64.dll
 #   ${CMAKE_SOURCE_DIR}/dependencies/OpenSSL/bin/libcrypto-3-x64.dll
 #   DESTINATION .
#)

# i nclude(CPack)
