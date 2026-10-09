# tabletinput: entrada del lápiz por WM_POINTER (Windows Ink), Win32 puro, sin Qt.
# La usan Cartuchera y, más adelante, paintcore (HU-46).
add_library(tabletinput STATIC
    include/tabletinput/PenReader.h
    src/PenReader.cpp
)
target_include_directories(tabletinput PUBLIC include)
target_link_libraries(tabletinput PUBLIC user32)
et_target_defaults(tabletinput)
set_target_properties(tabletinput PROPERTIES AUTOMOC OFF)

add_subdirectory(tests)
