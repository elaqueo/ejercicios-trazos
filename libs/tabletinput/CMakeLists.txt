# tabletinput: entrada del lápiz por WM_POINTER (Windows Ink), Win32 puro, sin Qt.
# La usa libs/lienzo (Cartuchera y Ejercicios).
add_library(tabletinput STATIC
    include/tabletinput/PenReader.h
    src/PenReader.cpp
    include/tabletinput/TouchRing.h
    src/TouchRing.cpp
)
target_include_directories(tabletinput PUBLIC include)
target_link_libraries(tabletinput PUBLIC user32)
et_target_defaults(tabletinput)
set_target_properties(tabletinput PROPERTIES AUTOMOC OFF)

add_subdirectory(tests)
