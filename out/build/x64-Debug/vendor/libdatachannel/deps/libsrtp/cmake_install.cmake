# Install script for directory: C:/Dev/RunicVTT/vendor/libdatachannel/deps/libsrtp

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Dev/RunicVTT/out/install/x64-Debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/deps/libsrtp/srtp2.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/deps/libsrtp/CMakeFiles/srtp2.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/srtp2" TYPE FILE FILES
    "C:/Dev/RunicVTT/vendor/libdatachannel/deps/libsrtp/include/srtp.h"
    "C:/Dev/RunicVTT/vendor/libdatachannel/deps/libsrtp/crypto/include/auth.h"
    "C:/Dev/RunicVTT/vendor/libdatachannel/deps/libsrtp/crypto/include/cipher.h"
    "C:/Dev/RunicVTT/vendor/libdatachannel/deps/libsrtp/crypto/include/crypto_types.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libSRTP/libSRTPTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libSRTP/libSRTPTargets.cmake"
         "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/deps/libsrtp/CMakeFiles/Export/4057af6c16ac9b4df3236859d5c9bdc7/libSRTPTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libSRTP/libSRTPTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libSRTP/libSRTPTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libSRTP" TYPE FILE FILES "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/deps/libsrtp/CMakeFiles/Export/4057af6c16ac9b4df3236859d5c9bdc7/libSRTPTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libSRTP" TYPE FILE FILES "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/deps/libsrtp/CMakeFiles/Export/4057af6c16ac9b4df3236859d5c9bdc7/libSRTPTargets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libSRTP" TYPE FILE FILES
    "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/deps/libsrtp/libSRTPConfig.cmake"
    "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/deps/libsrtp/libSRTPConfigVersion.cmake"
    )
endif()

