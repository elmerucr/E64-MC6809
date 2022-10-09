add_library(machine STATIC machine.cpp)

target_link_libraries(machine blitter cia lua MC6809 mmu sound timer)
