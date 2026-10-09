# Ejercicios de trazos sobre el lienzo de baja latencia (libs/lienzo, HU-65). La lógica de
# la app va en una biblioteca estática para que los tests la enlacen; el ejecutable es solo
# main.cpp.
qt_add_library(ejercicios_core STATIC
    src/ExerciseCanvas.h
    src/ExerciseMenu.h
    src/ExerciseMenu.cpp
    src/ExerciseSession.h
    src/ExerciseSession.cpp
    src/exercises/Exercise.h
    src/exercises/Exercise.cpp
    src/exercises/Hatching.h
    src/exercises/Hatching.cpp
    src/exercises/Curva.h
    src/exercises/Curva.cpp
    src/exercises/Recta.h
    src/exercises/Recta.cpp
)
target_include_directories(ejercicios_core PUBLIC src)
target_link_libraries(ejercicios_core PUBLIC appkit)
et_target_defaults(ejercicios_core)

qt_add_executable(ejercicios WIN32
    src/main.cpp
)
target_link_libraries(ejercicios PRIVATE ejercicios_core lienzo)
et_target_defaults(ejercicios)
et_deploy_qt(ejercicios)

add_subdirectory(tests)
