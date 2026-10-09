# Spike HU-44: swapchain D3D11 flip model en un HWND hijo de una ventana Qt, y latencia
# muestra → vsync. Código descartable.
qt_add_executable(cartuchera-swapchain WIN32 main.cpp)
target_link_libraries(cartuchera-swapchain PRIVATE Qt6::Widgets d3d11 dxgi d3dcompiler dwmapi user32 gdi32 shell32 ole32)
et_target_defaults(cartuchera-swapchain)
# Junto a ejercicios.exe, que ya tiene el Qt desplegado.
set_target_properties(cartuchera-swapchain PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
