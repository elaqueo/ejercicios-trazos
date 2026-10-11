# Mesa de animación: rough animation con hojas apiladas y flip a mano
# (docs/mesa-de-animacion/). El lienzo vive en libs/lienzo; el exe es solo main.cpp y su ícono.
qt_add_executable(mesadeanimacion WIN32 src/main.cpp mesadeanimacion.rc)
target_link_libraries(mesadeanimacion PRIVATE lienzo)
et_target_defaults(mesadeanimacion)
et_deploy_qt(mesadeanimacion)
