# Spike HU-81 (Mesa de animación, Fase 0): cuánto cuesta cambiar de hoja (tiles, renderTone,
# imagen guardada, GPU). Consola. Medir con el preset release. Código descartable.
qt_add_executable(mesa-flip main.cpp)
target_link_libraries(mesa-flip PRIVATE lienzo drymedia d3d11 psapi)
et_target_defaults(mesa-flip)
# Junto a los ejecutables de las apps, que ya tienen el Qt desplegado.
set_target_properties(mesa-flip PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
