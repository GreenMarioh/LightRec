# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-src")
  file(MAKE_DIRECTORY "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-build"
  "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-subbuild/ffnvcodec-populate-prefix"
  "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-subbuild/ffnvcodec-populate-prefix/tmp"
  "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-subbuild/ffnvcodec-populate-prefix/src/ffnvcodec-populate-stamp"
  "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-subbuild/ffnvcodec-populate-prefix/src"
  "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-subbuild/ffnvcodec-populate-prefix/src/ffnvcodec-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-subbuild/ffnvcodec-populate-prefix/src/ffnvcodec-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/mohni/Desktop/LightRec/build/_deps/ffnvcodec-subbuild/ffnvcodec-populate-prefix/src/ffnvcodec-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
