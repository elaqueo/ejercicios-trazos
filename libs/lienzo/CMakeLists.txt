# lienzo: el lienzo de baja latencia de las apps de medios secos (HU-63, nacido en
# Cartuchera): ventana nativa con swapchain, simulación y render en hilos propios, hoja =
# tableta, minas y goma de medios.json. Lo usan Cartuchera y Ejercicios.
qt_add_library(lienzo STATIC
    include/lienzo/Bench.h
    include/lienzo/CanvasWindow.h
    include/lienzo/Compositor.h
    include/lienzo/Cursor.h
    include/lienzo/DisplayImage.h
    include/lienzo/LeadController.h
    include/lienzo/LeadPicker.h
    include/lienzo/Lienzo.h
    include/lienzo/Media.h
    include/lienzo/Renderer.h
    include/lienzo/SampleQueue.h
    include/lienzo/SheetMapping.h
    include/lienzo/SheetStack.h
    include/lienzo/Simulation.h
    include/lienzo/Timing.h
    include/lienzo/Tone.h
    include/lienzo/ToolPage.h
    include/lienzo/ViewRotation.h
    src/Bench.cpp
    src/CanvasWindow.cpp
    src/Compositor.cpp
    src/Cursor.cpp
    src/LeadPicker.cpp
    src/Lienzo.cpp
    src/Media.cpp
    src/Renderer.cpp
    src/SheetMapping.cpp
    src/SheetStack.cpp
    src/Simulation.cpp
    src/Tone.cpp
    src/ToolPage.cpp
)
# Imágenes de la interfaz (encabezado del selector de lápices, pedido del usuario).
qt_add_resources(lienzo lienzo_imagenes PREFIX /lienzo BASE resources FILES resources/grafito.jpg)
target_include_directories(lienzo PUBLIC include)
target_link_libraries(lienzo PUBLIC drymedia tabletinput appkit d3d11 d3dcompiler dxgi dwmapi user32 gdi32)
et_target_defaults(lienzo)

add_subdirectory(tests)
