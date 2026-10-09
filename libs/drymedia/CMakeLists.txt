# drymedia: motor de medios secos de Cartuchera. Sin Qt ni D3D; solo la STL.
add_library(drymedia STATIC
    include/drymedia/Paper.h
    src/Paper.cpp
)
target_include_directories(drymedia PUBLIC include)
et_target_defaults(drymedia)
set_target_properties(drymedia PROPERTIES AUTOMOC OFF)

add_subdirectory(tests)
