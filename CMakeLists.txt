cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(CosmicArc C CXX ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(CosmicArc)
rp6502_asset(CosmicArc 0x10000 images/CosmicArc.bin)
rp6502_asset(CosmicArc 0x12000 images/StarMap.bin)
rp6502_asset(CosmicArc 0x12300 images/StarMap_TileMap.bin)
rp6502_asset(CosmicArc help src/help.txt)

rp6502_executable(CosmicArc
    DATA file 
    RESET file
)

target_sources(CosmicArc PRIVATE
    src/main.c
    src/mothership.c
)
