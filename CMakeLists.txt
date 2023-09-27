#Invocation for final build
#cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake_minimum_required(VERSION 3.16.0) #Because this was the version installed in mine
cmake_policy(SET CMP0048 NEW)

macro(default_to var value)
  if(NOT DEFINED ${var})
    set(${var} ${value})
  endif()
endmacro()

default_to(CMAKE_BUILD_TYPE Debug)

project(simc VERSION 0.1 LANGUAGES C CXX)
add_library(simc_options INTERFACE)

default_to(LOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/log)
default_to(LOG_LEVEL Debug)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  target_compile_definitions(simc_options INTERFACE SIMDEBUG LOG_DIR="${LOG_DIR}")
 
  if(LOG_LEVEL STREQUAL Warn)
    set(_LOG_LEVEL 0)
  elseif(LOG_LEVEL STREQUAL Info)
    set(_LOG_LEVEL 1)
  elseif(LOG_LEVEL STREQUAL Debug)
    set(_LOG_LEVEL 2) 
  else()
    message(FATAL_ERROR "Wrong log level specified!")
  endif()

  target_compile_definitions(simc_options INTERFACE LOG_LEVEL=${_LOG_LEVEL})
endif()

message(STATUS "Compiler is ${CMAKE_CXX_COMPILER_ID}")
target_compile_definitions(simc_options INTERFACE ${CMAKE_CXX_COMPILER_ID})

set(COMPILER ${CMAKE_CXX_COMPILER_ID})

if(${COMPILER} STREQUAL MSVC)
  set(std_opt "/std:c++20" "/Zc:preprocessor") 
elseif(${COMPILER} STREQUAL GNU OR ${COMPILER} STREQUAL Clang)
  if(DEFINED CMAKE_CXX_COMPILER_VERSION AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 10)
    set(std_opt "-std=c++2a")
  else()
    set(std_opt "-std=c++20")
  endif()
endif()

if(DEFINED std_opt)
  target_compile_options(simc_options INTERFACE ${std_opt})
endif()

set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
target_include_directories(simc_options INTERFACE ${INCLUDE_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

#Our cmake script automatically fetches and builds an executable for every subdirectory in the src directory
macro(subdirlist result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

subdirlist(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)

foreach(DIR ${SUBDIRS})
    FILE(GLOB srcs src/${DIR}/*.cpp)

    if(DIR STREQUAL simcc)
      add_subdirectory(src/simcc)
    else()
      add_executable(${DIR} ${srcs} src/entry.cpp src/options.cpp)
    endif()
    
    target_link_libraries(${DIR} simc_options)
    target_compile_definitions(${DIR} PRIVATE MODULENAME="${DIR}")
endforeach()

#If building in vs, this sets the startup project to simcc
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT simcc)

message(STATUS "Build type:${CMAKE_BUILD_TYPE}")
