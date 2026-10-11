# Lámina del papel de animación (HU-97): el mismo dibujo en el papel de Cartuchera y en el de
# Mesa de animación con varias profundidades de grano, al 100 y al 200 %. Escribe un PNG.
qt_add_executable(mesa-papel main.cpp)
target_link_libraries(mesa-papel PRIVATE lienzo drymedia appkit)
et_target_defaults(mesa-papel)
set_target_properties(mesa-papel PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
