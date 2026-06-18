# Install script for directory: C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/LightRec")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
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

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/vpl/examples" TYPE DIRECTORY FILES "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/content")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/vpl/examples/api1x_core" TYPE DIRECTORY FILES
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api1x_core/legacy-decode"
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api1x_core/legacy-vpp"
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api1x_core/legacy-encode"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/vpl/examples/interop" TYPE DIRECTORY FILES "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/interop/vpl-infer")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/vpl/examples/api2x" TYPE DIRECTORY FILES
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api2x/hello-decode"
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api2x/hello-decvpp"
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api2x/hello-encode"
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api2x/hello-transcode"
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api2x/hello-vpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/vpl/examples/tutorials" TYPE DIRECTORY FILES "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/tutorials/01_transition")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/vpl/examples/api2x" TYPE DIRECTORY FILES
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api2x/hello-sharing-dx11"
    "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-src/examples/api2x/hello-sharing-ocl"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/mohni/Desktop/LightRec/build/_deps/libvpl-build/examples/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
