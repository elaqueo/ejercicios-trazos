# Overlay del port oficial de vcpkg (tag 2026.07.29). Difiere en tres cosas:
# - Compila SIN glib. libmypaint solo usa glib para GEGL/introspection, que no usamos,
#   y así evitamos arrastrar glib (LGPL), gettext, pcre2 y libffi a un enlace estático.
# - fix_unknown_ids_msvc.patch: con MSVC, un .myb con ajustes o entradas desconocidos
#   corrompía el heap en vez de saltear ese ajuste (ver el encabezado del parche).
#   Reportado upstream: https://github.com/mypaint/libmypaint/issues/209
# - fix_uninitialized_brush.patch: el primer dab de un pincel nuevo leía memoria sin
#   inicializar y a veces el trazo no se pintaba (ver el encabezado del parche).

vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/mypaint/libmypaint/releases/download/v${VERSION}/libmypaint-${VERSION}.tar.xz"
    FILENAME "libmypaint-${VERSION}.tar.xz"
    SHA512 e9413fd6a5336791ab3228a5ad9e7f06871d075c7ded236942f896a205ba44ea901a945fdc97b8be357453a1505331b59e824fe67500fbcda0cc4f11f79af608
)

vcpkg_extract_source_archive(
    SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES
        fix_i18n.diff
        win_math.patch
        disable_tests.diff
        fix_unknown_ids_msvc.patch    # propio: ver el encabezado del parche
        fix_uninitialized_brush.patch # propio: ver el encabezado del parche
)

# El tarball trae un config.h pregenerado CON glib (MYPAINT_CONFIG_USE_GLIB 1). MSVC
# resuelve #include "config.h" primero en la carpeta del .c, así que ese archivo le
# gana al config.h que genera configure en la carpeta de build. Lo borramos.
file(REMOVE "${SOURCE_PATH}/config.h")

vcpkg_make_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    AUTORECONF
    OPTIONS
        --disable-i18n
        --disable-introspection
        --without-glib
)

vcpkg_make_install()

vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
