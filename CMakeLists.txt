project(eforce)
cmake_minimum_required(VERSION 3.10)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11")

include(ExternalProject)

set(EXTERNAL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/ext)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(SYSTEM ${EXTERNAL_PREFIX}/include)

set(LIB_FILES 
  src/Elf.cpp
  src/ExceptionForcer.cpp
  src/OpcodeGeneratorX64.cpp
  src/OpcodeGeneratorThumb.cpp
  src/OpcodeGeneratorAarch64.cpp
)

add_library(eforce ${LIB_FILES})
target_link_libraries(eforce bfd iberty z dl)

install(TARGETS eforce 
  ARCHIVE
	DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/eforce 
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include
  FILES_MATCHING PATTERN *.h)

ExternalProject_Add(catch2
	URL https://github.com/catchorg/Catch2/releases/download/v2.1.2/catch.hpp
	DOWNLOAD_NO_EXTRACT ON
	DOWNLOAD_NAME "catch.hpp"
	DOWNLOAD_DIR ${EXTERNAL_PREFIX}/include
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ""
	INSTALL_COMMAND "")

add_executable(test_prog test/test_prog.cpp test/ExceptionForcerTest.cpp)
add_dependencies(test_prog catch2)
target_link_libraries(test_prog eforce)

find_package(Doxygen REQUIRED dot)

file(GLOB_RECURSE HEADERS include/*.h)
doxygen_add_docs(eforce_doc ${HEADERS} ${LIB_FILES})

