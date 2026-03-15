cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(CosmicArc C CXX ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(CosmicArc)
rp6502_asset(CosmicArc 0x10000 images/MainScreen_tiles.bin)
rp6502_asset(CosmicArc 0x11BC0 images/MainScreen_map.bin)
rp6502_asset(CosmicArc 0x12070 images/Mothership_map.bin)
rp6502_asset(CosmicArc 0x12520 images/laser.bin)
rp6502_asset(CosmicArc 0x13520 images/Asteroid.bin)
rp6502_asset(CosmicArc help src/help.txt)

rp6502_executable(CosmicArc
    DATA file 
    RESET file
)

target_sources(CosmicArc PRIVATE
    src/main.c
    src/starfield.c
    src/mothership.c
    src/laser.c
    src/asteroid.c
    src/score.c
    src/input.c
)

add_executable(GamepadMapper)
rp6502_executable(GamepadMapper
    DATA file
    RESET file
)

target_sources(GamepadMapper PRIVATE
    src/gamepad_mapper.c
    src/input.c
)
