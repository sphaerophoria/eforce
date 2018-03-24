project(eforce)
cmake_minimum_required(VERSION 2.8)

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

# Prepare "Catch" library for other executables
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/catch)
add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ${CATCH_INCLUDE_DIR})

add_executable(test_prog test/test_prog.cpp test/ExceptionForcerTest.cpp)
target_link_libraries(test_prog eforce Catch)

