# Install script for directory: C:/Dev/RunicVTT/vendor/libdatachannel

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/datachannel.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Dev/RunicVTT/bin/datachannel.dll")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rtc" TYPE FILE FILES
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/candidate.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/channel.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/configuration.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/datachannel.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/description.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/mediahandler.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtcpreceivingsession.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/common.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/global.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/message.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/frameinfo.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/peerconnection.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/reliability.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtc.h"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtc.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtp.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/track.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/websocket.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/websocketserver.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtppacketizationconfig.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtcpsrreporter.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtppacketizer.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtpdepacketizer.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/h264rtppacketizer.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/h264rtpdepacketizer.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/nalunit.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/h265rtppacketizer.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/h265rtpdepacketizer.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/h265nalunit.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/av1rtppacketizer.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rtcpnackresponder.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/utils.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/plihandler.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/pacinghandler.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/rembhandler.hpp"
    "C:/Dev/RunicVTT/vendor/libdatachannel/include/rtc/version.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets.cmake"
         "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/CMakeFiles/Export/32c821eb1e7b36c3a3818aec162f7fd2/LibDataChannelTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel" TYPE FILE FILES "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/CMakeFiles/Export/32c821eb1e7b36c3a3818aec162f7fd2/LibDataChannelTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel" TYPE FILE FILES "C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/CMakeFiles/Export/32c821eb1e7b36c3a3818aec162f7fd2/LibDataChannelTargets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel" TYPE FILE FILES
    "C:/Dev/RunicVTT/out/build/x64-Debug/LibDataChannelConfig.cmake"
    "C:/Dev/RunicVTT/out/build/x64-Debug/LibDataChannelConfigVersion.cmake"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/client/cmake_install.cmake")
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/client-benchmark/cmake_install.cmake")
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/media-receiver/cmake_install.cmake")
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/media-sender/cmake_install.cmake")
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/media-sfu/cmake_install.cmake")
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/streamer/cmake_install.cmake")
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/copy-paste/cmake_install.cmake")
  include("C:/Dev/RunicVTT/out/build/x64-Debug/vendor/libdatachannel/examples/copy-paste-capi/cmake_install.cmake")

endif()

