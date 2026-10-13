qt_add_library(cast_fake STATIC FakeRenderer.h FakeRenderer.cpp)
target_include_directories(cast_fake PUBLIC .)
target_link_libraries(cast_fake PUBLIC cast)
et_target_defaults(cast_fake)

et_add_qt_test(tst_cast SOURCES tst_cast.cpp LIBS cast_fake)

# El TV de mentira como programa, para la prueba de la app real (scripts/smoke.ps1): junto a los
# exe de las apps, con el Qt que despliegan.
qt_add_executable(fake_renderer fake_renderer.cpp)
target_link_libraries(fake_renderer PRIVATE cast_fake)
et_target_defaults(fake_renderer)
