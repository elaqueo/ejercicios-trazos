# La lógica de la app va en una biblioteca estática para que los tests la enlacen;
# el ejecutable es solo main.cpp.
qt_add_library(ejercicios_core STATIC
    src/ExerciseSession.h
    src/ExerciseSession.cpp
    src/exercises/Exercise.h
    src/exercises/Exercise.cpp
    src/exercises/Recta.h
    src/exercises/Recta.cpp
)
target_include_directories(ejercicios_core PUBLIC src)
target_link_libraries(ejercicios_core PUBLIC appkit)
et_target_defaults(ejercicios_core)

qt_add_executable(ejercicios WIN32
    src/main.cpp
)
target_link_libraries(ejercicios PRIVATE ejercicios_core)
et_target_defaults(ejercicios)
et_deploy_qt(ejercicios)
et_deploy_brushes(ejercicios)

add_subdirectory(tests)
