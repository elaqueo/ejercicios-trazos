et_add_qt_test(tst_bocetos SOURCES tst_bocetos.cpp LIBS bocetos_core Qt6::CorePrivate)

# HU-142: el Krita de mentira, para la prueba de la app real (scripts/smoke.ps1): "dibuja" en la
# hoja (un rectángulo oscuro en la imagen) y la guarda como Krita, sin abrir ventanas.
qt_add_executable(fake_krita WIN32 fake_krita.cpp) # sin consola: no le saca el foco a Bocetos
target_link_libraries(fake_krita PRIVATE Qt6::Gui Qt6::CorePrivate)
et_target_defaults(fake_krita)
