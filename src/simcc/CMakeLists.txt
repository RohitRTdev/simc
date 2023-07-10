default_to(ARCH x64)

FILE(GLOB lib_srcs ${PROJECT_SOURCE_DIR}/lib/${ARCH}/*.cpp)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(lib_srcs ${lib_srcs} ${PROJECT_SOURCE_DIR}/lib/debug.cpp)
endif()

add_library(code-gen SHARED ${lib_srcs})
target_link_libraries(code-gen simc_options)


FILE(GLOB mod_srcs *.cpp)
add_executable(simcc ${mod_srcs} ${PROJECT_SOURCE_DIR}/src/entry.cpp)
target_compile_definitions(simcc PRIVATE INITDEBUGGER)

target_link_libraries(simcc code-gen)
