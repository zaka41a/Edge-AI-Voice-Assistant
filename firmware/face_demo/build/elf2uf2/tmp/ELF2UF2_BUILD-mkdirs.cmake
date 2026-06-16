# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/zakaria/Documents/Mikrocontrollertechnik/yahal_with_examples/YAHAL/tools/elf2uf2")
  file(MAKE_DIRECTORY "/Users/zakaria/Documents/Mikrocontrollertechnik/yahal_with_examples/YAHAL/tools/elf2uf2")
endif()
file(MAKE_DIRECTORY
  "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2"
  "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2"
  "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2/tmp"
  "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2/src/ELF2UF2_BUILD-stamp"
  "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2/src"
  "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2/src/ELF2UF2_BUILD-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2/src/ELF2UF2_BUILD-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/zakaria/Documents/Mikrocontrollertechnik/Project/firmware/face_demo/build/elf2uf2/src/ELF2UF2_BUILD-stamp${cfgdir}") # cfgdir has leading slash
endif()
