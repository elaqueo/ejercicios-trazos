# Cartuchera: app de medios secos (docs/medios-secos/arquitectura.md). El lienzo vive en
# libs/lienzo (HU-63); el exe es solo main.cpp.
qt_add_executable(cartuchera WIN32 src/main.cpp cartuchera.rc)
target_link_libraries(cartuchera PRIVATE lienzo)
et_target_defaults(cartuchera)
et_deploy_qt(cartuchera)
