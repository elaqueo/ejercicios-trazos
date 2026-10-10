# Mesa de animación: rough animation con hojas apiladas y flip a mano
# (docs/mesa-de-animacion/). El lienzo vive en libs/lienzo; el exe es solo main.cpp. El ícono
# y el instalador llegan con HU-110.
qt_add_executable(mesadeanimacion WIN32 src/main.cpp)
target_link_libraries(mesadeanimacion PRIVATE lienzo)
et_target_defaults(mesadeanimacion)
et_deploy_qt(mesadeanimacion)
