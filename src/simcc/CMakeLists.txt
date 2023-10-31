default_to(ARCH x64)

if(ARCH STREQUAL x64)
    set(ARCH_DEF ARCH_X64)
endif()

FILE(GLOB lib_srcs ${PROJECT_SOURCE_DIR}/lib/${ARCH}/*.cpp)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(lib_srcs ${lib_srcs} ${PROJECT_SOURCE_DIR}/lib/debug.cpp)
endif()

add_library(code-gen SHARED ${lib_srcs})
target_link_libraries(code-gen simc_options)
target_compile_definitions(code-gen PRIVATE BUILD_LIB)
target_compile_definitions(code-gen PUBLIC ${ARCH_DEF})

if(${COMPILER} STREQUAL GNU OR ${COMPILER} STREQUAL Clang)
    target_compile_options(code-gen PRIVATE "-fvisibility=hidden")
endif()

FILE(GLOB mod_srcs_debug *.cpp */*.cpp)
string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/ast/debug.cpp;" "" mod_srcs_rel "${mod_srcs_debug}")

if(CMAKE_BUILD_TYPE STREQUAL Release)
    set(mod_srcs ${mod_srcs_rel})    
else()
    set(mod_srcs ${mod_srcs_debug})
endif()


string(REPLACE " " ";" COMMON_FILES_LIST "${COMMON_FILES}")
foreach(STRING ${COMMON_FILES_LIST})
  list(APPEND APPEND_FILE_LIST "${PROJECT_SOURCE_DIR}/${STRING}")
endforeach()
string(JOIN " " APPEND_FILES "${APPEND_FILE_LIST}")

add_executable(simcc ${mod_srcs} ${APPEND_FILE_LIST})
target_compile_definitions(simcc PRIVATE INITDEBUGGER)

target_link_libraries(simcc code-gen)
