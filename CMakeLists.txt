cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(CosmicArc C CXX ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(CosmicArc)
rp6502_asset(CosmicArc 0x10000 images/MainScreen_tiles.bin)
rp6502_asset(CosmicArc 0x12FC0 images/MainScreen_map.bin)
rp6502_asset(CosmicArc 0x13470 images/Mothership_map.bin)
rp6502_asset(CosmicArc 0x13920 images/laser.bin)
rp6502_asset(CosmicArc 0x14920 images/Asteroid.bin)
rp6502_asset(CosmicArc 0x14B20 images/beasties.bin)
rp6502_asset(CosmicArc 0x15520 images/lander.bin)
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
    src/beasties.c
    src/score.c
    src/input.c
    src/opl.c
    src/sound.c
    src/instruments.c
    src/lander.c
)

target_compile_definitions(CosmicArc PRIVATE USE_NATIVE_OPL2)

add_executable(GamepadMapper)
rp6502_executable(GamepadMapper
    DATA file
    RESET file
)

target_sources(GamepadMapper PRIVATE
    src/gamepad_mapper.c
    src/input.c
)
