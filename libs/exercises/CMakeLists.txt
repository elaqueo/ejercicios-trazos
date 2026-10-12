# exercises: los ejercicios de trazos (HU-118, nacidos en apps/ejercicios): Exercise y sus
# generadores deterministas por semilla, la sesión (siguiente, repetir, modo mixto) y el menú
# F4. Los usan Ejercicios de trazos y Ejercicios de animación.
qt_add_library(exercises STATIC
    include/exercises/CabezaQueGira.h
    include/exercises/Cajas.h
    include/exercises/CilindroQueGira.h
    include/exercises/CajasRotadas.h
    include/exercises/Concentricas.h
    include/exercises/ConoQueGira.h
    include/exercises/Curva.h
    include/exercises/CurvaDireccion.h
    include/exercises/Direccion.h
    include/exercises/Elipse.h
    include/exercises/ElipsePerspectiva.h
    include/exercises/Exercise.h
    include/exercises/ExerciseCanvas.h
    include/exercises/ExerciseMenu.h
    include/exercises/ExerciseSession.h
    include/exercises/Ghosting.h
    include/exercises/Hatching.h
    include/exercises/Parrafo.h
    include/exercises/Presion.h
    include/exercises/PrismaQueGira.h
    include/exercises/Radiales.h
    include/exercises/Recta.h
    include/exercises/Renglon.h
    include/exercises/RenglonCurvo.h
    include/exercises/StackExercise.h
    include/exercises/StackSession.h
    include/exercises/StrokeStacks.h
    include/exercises/Turntable.h
    include/exercises/WritingCurve.h
    src/CabezaQueGira.cpp
    src/Cajas.cpp
    src/CilindroQueGira.cpp
    src/CajasRotadas.cpp
    src/Concentricas.cpp
    src/ConoQueGira.cpp
    src/Curva.cpp
    src/CurvaDireccion.cpp
    src/Direccion.cpp
    src/Elipse.cpp
    src/ElipsePerspectiva.cpp
    src/Exercise.cpp
    src/ExerciseMenu.cpp
    src/ExerciseSession.cpp
    src/Ghosting.cpp
    src/Hatching.cpp
    src/Parrafo.cpp
    src/Presion.cpp
    src/PrismaQueGira.cpp
    src/Radiales.cpp
    src/Recta.cpp
    src/Renglon.cpp
    src/RenglonCurvo.cpp
    src/StackExercise.cpp
    src/StackSession.cpp
    src/StrokeStacks.cpp
    src/Turntable.cpp
    src/WritingCurve.cpp
)
target_include_directories(exercises PUBLIC include)
target_link_libraries(exercises PUBLIC appkit)
et_target_defaults(exercises)

add_subdirectory(tests)
