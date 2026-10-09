# Spike HU-45: benchmark del kernel de contacto (escalar, AVX2, AVX-512). Consola, sin
# Qt. Medir con el preset release: en Debug el código no está optimizado.
add_executable(cartuchera-kernel main.cpp kernel.h kernel_scalar.cpp kernel_avx2.cpp kernel_avx512.cpp)
et_target_defaults(cartuchera-kernel)
# Cada ruta se compila para su conjunto de instrucciones; main.cpp elige en tiempo de
# ejecución según la CPU (así el mismo exe corre en la desktop sin AVX-512).
set_source_files_properties(kernel_avx2.cpp PROPERTIES COMPILE_OPTIONS /arch:AVX2)
set_source_files_properties(kernel_avx512.cpp PROPERTIES COMPILE_OPTIONS /arch:AVX512)
set_target_properties(cartuchera-kernel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/spikes"
    AUTOMOC OFF)
