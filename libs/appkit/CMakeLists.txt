# appkit: framework común de app (ventana, área útil, overlays, config, atajos).
# Depende de paintcore; nunca al revés.
qt_add_library(appkit STATIC
    include/appkit/AppWindow.h
    include/appkit/Log.h
    include/appkit/Paths.h
    src/AppWindow.cpp
    src/Log.cpp
    src/Paths.cpp
)
target_include_directories(appkit PUBLIC include)
target_link_libraries(appkit PUBLIC paintcore Qt6::Widgets)
et_target_defaults(appkit)
