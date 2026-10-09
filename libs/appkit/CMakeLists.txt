# appkit: framework común de app (área útil, overlays, config, atajos, guías, tema).
qt_add_library(appkit STATIC
    include/appkit/CalibrationOverlay.h
    include/appkit/Config.h
    include/appkit/Guides.h
    include/appkit/Log.h
    include/appkit/Paths.h
    include/appkit/ScreenChoice.h
    include/appkit/Shortcuts.h
    include/appkit/Theme.h
    include/appkit/UsableArea.h
    src/CalibrationOverlay.cpp
    src/Config.cpp
    src/Guides.cpp
    src/Log.cpp
    src/Paths.cpp
    src/ScreenChoice.cpp
    src/Shortcuts.cpp
    src/UsableArea.cpp
)
target_include_directories(appkit PUBLIC include)
target_link_libraries(appkit PUBLIC Qt6::Widgets)
et_target_defaults(appkit)

add_subdirectory(tests)
