qt_add_executable(ejercicios WIN32
    src/main.cpp
)
target_link_libraries(ejercicios PRIVATE appkit)
et_target_defaults(ejercicios)
et_deploy_qt(ejercicios)
