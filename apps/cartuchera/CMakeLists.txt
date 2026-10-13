# Cartuchera: app de medios secos (docs/medios-secos/arquitectura.md). El lienzo vive en
# libs/lienzo (HU-63); el exe es main.cpp y el modo cuaderno de Bocetos (HU-133), que va en
# cartuchera_core para probarlo.
qt_add_library(cartuchera_core STATIC src/NotebookMode.h src/NotebookMode.cpp)
target_include_directories(cartuchera_core PUBLIC src)
target_link_libraries(cartuchera_core PUBLIC notebooks)
et_target_defaults(cartuchera_core)

qt_add_executable(cartuchera WIN32 src/main.cpp cartuchera.rc)
target_link_libraries(cartuchera PRIVATE cartuchera_core lienzo)
et_target_defaults(cartuchera)
et_deploy_qt(cartuchera)

add_subdirectory(tests)
