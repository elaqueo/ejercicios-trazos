# La biblioteca no usa Qt; las pruebas usan Qt Test como el resto del repo.
et_add_qt_test(tst_paper SOURCES tst_paper.cpp LIBS drymedia)
et_add_qt_test(tst_contact SOURCES tst_contact.cpp LIBS drymedia)
et_add_qt_test(tst_pencil SOURCES tst_pencil.cpp LIBS drymedia)
