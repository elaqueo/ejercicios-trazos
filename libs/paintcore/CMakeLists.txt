# paintcore: motor de pinceles. No conoce a appkit (RNF-05) y no expone tipos de
# libmypaint en sus headers públicos.

qt_add_library(paintcore STATIC
    include/paintcore/CanvasWidget.h
    src/Brush.cpp
    src/Brush.h
    src/CanvasWidget.cpp
    src/Surface.cpp
    src/Surface.h
)
target_include_directories(paintcore PUBLIC include PRIVATE src)
target_link_libraries(paintcore
    PUBLIC Qt6::Widgets
    # PRIVATE: los headers de libmypaint no llegan a appkit ni a las apps. Como
    # paintcore es estática, CMake igual propaga el .lib al enlace final (LINK_ONLY).
    PRIVATE LibMyPaint::LibMyPaint)
et_target_defaults(paintcore)

add_subdirectory(tests)
