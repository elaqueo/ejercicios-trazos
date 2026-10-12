# Ejercicios de animación: ejercicios en pila con la mesa de luz y el flip de la Mesa
# (docs/ejercicios-animacion/). El lienzo vive en libs/lienzo y los ejercicios en
# libs/exercises; el exe es solo main.cpp y su ícono.
qt_add_executable(ejerciciosanimacion WIN32 src/main.cpp ejerciciosanimacion.rc)
target_link_libraries(ejerciciosanimacion PRIVATE lienzo exercises)
et_target_defaults(ejerciciosanimacion)
et_deploy_qt(ejerciciosanimacion)
