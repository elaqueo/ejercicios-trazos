# paintcore: motor de pinceles. No conoce a appkit (RNF-05) y no expone tipos de
# libmypaint en sus headers públicos.
qt_add_library(paintcore STATIC
    include/paintcore/CanvasWidget.h
    src/CanvasWidget.cpp
)
target_include_directories(paintcore PUBLIC include)
target_link_libraries(paintcore PUBLIC Qt6::Widgets)
et_target_defaults(paintcore)
