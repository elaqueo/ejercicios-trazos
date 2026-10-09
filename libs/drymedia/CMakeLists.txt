# drymedia: motor de medios secos de Cartuchera. Sin Qt ni D3D; solo la STL.
add_library(drymedia STATIC
    include/drymedia/Contact.h
    include/drymedia/History.h
    include/drymedia/Medium.h
    include/drymedia/Paper.h
    include/drymedia/Pencil.h
    include/drymedia/Tip.h
    src/Contact.cpp
    src/History.cpp
    src/kernel.h
    src/kernel_avx2.cpp
    src/kernel_scalar.cpp
    src/Paper.cpp
    src/Pencil.cpp
    src/Tip.cpp
)
target_include_directories(drymedia PUBLIC include)
et_target_defaults(drymedia)
# La ruta AVX2 se compila para AVX2; Contact elige en tiempo de ejecución según la CPU
# (spike HU-45: AVX-512 no mejora y quedó descartado).
set_source_files_properties(src/kernel_avx2.cpp PROPERTIES COMPILE_OPTIONS /arch:AVX2)
set_target_properties(drymedia PROPERTIES AUTOMOC OFF)

add_subdirectory(tests)
