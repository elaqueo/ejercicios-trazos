# Bocetos (docs/bocetos/): la biblioteca de cuadernos de bocetos que se llenan en Cartuchera. Una
# app Qt común, sin libs/lienzo (RNF-06): muestra PNG. bocetos_core tiene lo que se prueba.
qt_add_library(bocetos_core STATIC src/BocetosCore.h src/BocetosCore.cpp)
target_include_directories(bocetos_core PUBLIC src)
target_link_libraries(bocetos_core PUBLIC notebooks appkit)
et_target_defaults(bocetos_core)

qt_add_executable(bocetos WIN32 src/main.cpp src/LibraryWindow.h src/LibraryWindow.cpp src/NotebookDialog.h src/NotebookDialog.cpp)
target_link_libraries(bocetos PRIVATE bocetos_core appkit)
et_target_defaults(bocetos)
et_deploy_qt(bocetos)

add_subdirectory(tests)
