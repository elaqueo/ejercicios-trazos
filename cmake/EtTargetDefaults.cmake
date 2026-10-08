# Opciones comunes de compilación para los targets propios del monorepo.

option(ET_WARNINGS_AS_ERRORS "Tratar los warnings como errores (Definition of Done: sin warnings nuevos)" ON)

function(et_target_defaults target)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    # /external:W0: los headers de Qt y de vcpkg (targets importados) no generan warnings.
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /external:W0)
    if(ET_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE /WX)
    endif()
endfunction()

# Crea un ejecutable de Qt Test y lo registra en CTest.
#   et_add_qt_test(tst_algo SOURCES tst_algo.cpp LIBS paintcore)
# Los tests van a build/<preset>/tests/ y no a la raíz, donde windeployqt deja solo el
# plugin qwindows: así cargan Qt desde su instalación (PATH) y usan la plataforma
# offscreen, que no abre ventanas.
function(et_add_qt_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBS")
    qt_add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${arg_LIBS})
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    et_target_defaults(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${ET_QT_PREFIX}/bin")
endfunction()

# Copia los pinceles de fábrica (port mypaint-brushes, CC0) a <carpeta del exe>/brushes,
# donde los busca appkit::brushDirectories().
function(et_deploy_brushes target)
    set(source "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/mypaint-brushes/mypaint-data/2.0/brushes")
    if(NOT IS_DIRECTORY "${source}")
        message(FATAL_ERROR "No se encontraron los pinceles de mypaint-brushes en ${source}")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${source}" "$<TARGET_FILE_DIR:${target}>/brushes"
        COMMENT "Copiando los pinceles de fábrica junto a $<TARGET_FILE_NAME:${target}>"
        VERBATIM)
endfunction()

# Copia el Qt Release y sus plugins junto al ejecutable, para correrlo desde build/.
function(et_deploy_qt target)
    find_program(ET_WINDEPLOYQT windeployqt HINTS "${ET_QT_PREFIX}/bin" NO_DEFAULT_PATH REQUIRED)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${ET_WINDEPLOYQT}" --release --no-translations --no-system-d3d-compiler
                --no-opengl-sw --no-compiler-runtime --verbose 0 "$<TARGET_FILE:${target}>"
        COMMENT "Desplegando Qt Release junto a $<TARGET_FILE_NAME:${target}>"
        VERBATIM)
endfunction()
