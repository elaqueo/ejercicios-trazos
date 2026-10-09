# Cartuchera: app de medios secos (docs/medios-secos/arquitectura.md). La lógica va en
# una biblioteca estática para que los tests la enlacen; el exe es solo main.cpp.
qt_add_library(cartuchera_core STATIC
    src/Bench.h
    src/Bench.cpp
    src/CanvasWindow.h
    src/CanvasWindow.cpp
    src/DisplayImage.h
    src/LeadController.h
    src/Media.h
    src/Media.cpp
    src/Renderer.h
    src/Renderer.cpp
    src/SampleQueue.h
    src/SheetMapping.h
    src/SheetMapping.cpp
    src/Simulation.h
    src/Simulation.cpp
    src/Tone.h
    src/Timing.h
    src/Tone.cpp
)
target_include_directories(cartuchera_core PUBLIC src)
target_link_libraries(cartuchera_core PUBLIC drymedia tabletinput appkit d3d11 dxgi dwmapi user32 gdi32)
et_target_defaults(cartuchera_core)

qt_add_executable(cartuchera WIN32 src/main.cpp)
target_link_libraries(cartuchera PRIVATE cartuchera_core)
et_target_defaults(cartuchera)
et_deploy_qt(cartuchera)

add_subdirectory(tests)
