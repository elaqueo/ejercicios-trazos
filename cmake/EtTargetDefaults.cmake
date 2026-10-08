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

# Copia el Qt Release y sus plugins junto al ejecutable, para correrlo desde build/.
function(et_deploy_qt target)
    find_program(ET_WINDEPLOYQT windeployqt HINTS "${ET_QT_PREFIX}/bin" NO_DEFAULT_PATH REQUIRED)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${ET_WINDEPLOYQT}" --release --no-translations --no-system-d3d-compiler
                --no-opengl-sw --no-compiler-runtime --verbose 0 "$<TARGET_FILE:${target}>"
        COMMENT "Desplegando Qt Release junto a $<TARGET_FILE_NAME:${target}>"
        VERBATIM)
endfunction()
