# Spike HU-43: entrada del lápiz por WM_POINTER, sin Qt. Código descartable.
add_executable(cartuchera-entrada WIN32 main.cpp)
# Desde HU-47 también pasa cada mensaje por tabletinput y registra su resultado, para
# validar la biblioteca contra los datos crudos.
target_link_libraries(cartuchera-entrada PRIVATE tabletinput user32 gdi32 shell32 ole32)
et_target_defaults(cartuchera-entrada)
set_target_properties(cartuchera-entrada PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/spikes"
    AUTOMOC OFF) # sin Qt
