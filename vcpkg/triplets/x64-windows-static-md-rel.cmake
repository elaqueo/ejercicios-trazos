# Bibliotecas estáticas con CRT dinámico (/MD), como el resto del proyecto y Qt.
# Solo Release: la configuración Debug del proyecto también consume la variante
# Release de los targets importados (CMAKE_MAP_IMPORTED_CONFIG_DEBUG), así que
# compilar Debug sería tiempo perdido.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
