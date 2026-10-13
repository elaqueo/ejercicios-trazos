# notebooks: el cuaderno de Bocetos (HU-132): cuaderno.json, el estado de las hojas y la
# biblioteca. Lo usan Bocetos y Cartuchera en modo cuaderno; solo Qt Core, sin el lienzo.
qt_add_library(notebooks STATIC
    include/notebooks/Library.h
    include/notebooks/Notebook.h
    src/Library.cpp
    src/Notebook.cpp
)
target_include_directories(notebooks PUBLIC include)
target_link_libraries(notebooks PUBLIC Qt6::Gui)
et_target_defaults(notebooks)

add_subdirectory(tests)
