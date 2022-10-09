cmake_minimum_required (VERSION 3.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O3")

project(E64)

find_package(sdl2 REQUIRED)

include_directories(
    ${SDL2_INCLUDE_DIRS}
    src/
    src/host/
    src/hud/
    src/lua-5.4.4/src/
    src/components/
    src/components/blitter/
    src/components/cia/
    src/components/MC6809/
    src/components/mmu/
    src/components/sound/
    src/components/sound/resid-0.16/
    src/components/timer/
    src/components/vicv/
    src/machine/
    src/rom/
)

add_subdirectory(src/)

add_executable(E64 src/main.cpp)

target_link_libraries(E64 host hud machine rom ${SDL2_LIBRARIES})
