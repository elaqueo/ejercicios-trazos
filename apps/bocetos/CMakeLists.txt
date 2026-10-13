# Bocetos (docs/bocetos/): la biblioteca de cuadernos de bocetos que se llenan en Cartuchera. Una
# app Qt común, sin libs/lienzo (RNF-06): muestra PNG. bocetos_core tiene lo que se prueba.
# HU-142: el .kra es un zip; QZipWriter y QZipReader son API privada de Qt Core (están desde Qt 4).
find_package(Qt6 ${ET_QT_VERSION} EXACT REQUIRED COMPONENTS CorePrivate)
qt_add_library(bocetos_core STATIC src/BocetosCore.h src/BocetosCore.cpp src/SheetCache.h src/SheetCache.cpp
               src/Kra.h src/Kra.cpp src/ThumbCache.h src/ThumbCache.cpp)
target_include_directories(bocetos_core PUBLIC src)
target_link_libraries(bocetos_core PUBLIC notebooks appkit cast PRIVATE Qt6::CorePrivate)
et_target_defaults(bocetos_core)

qt_add_executable(bocetos WIN32 src/main.cpp bocetos.rc src/LibraryWindow.h src/LibraryWindow.cpp src/NotebookDialog.h src/NotebookDialog.cpp
                  src/NotebookView.h src/NotebookView.cpp src/Paint.h src/Paint.cpp src/Notice.h src/Notice.cpp
                  src/TvLink.h src/TvLink.cpp src/TvChooser.h src/TvChooser.cpp)
target_link_libraries(bocetos PRIVATE bocetos_core appkit)
et_target_defaults(bocetos)
et_deploy_qt(bocetos)

add_subdirectory(tests)
