# Ejercicios de trazos sobre el lienzo de baja latencia (libs/lienzo, HU-65). Los ejercicios,
# su sesión y su menú están en libs/exercises (HU-118); el ejecutable es solo main.cpp.
qt_add_executable(ejercicios WIN32
    src/main.cpp
    ejercicios.rc
)
target_link_libraries(ejercicios PRIVATE exercises lienzo)
et_target_defaults(ejercicios)
et_deploy_qt(ejercicios)
