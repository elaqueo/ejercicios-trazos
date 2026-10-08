# Busca libmypaint (instalada por vcpkg, ver vcpkg/ports/libmypaint).
# libmypaint se compila con autotools y solo instala un .pc, sin config de CMake;
# este módulo crea el target importado LibMyPaint::LibMyPaint.

find_package(json-c CONFIG REQUIRED)

find_path(LibMyPaint_INCLUDE_DIR mypaint-brush.h PATH_SUFFIXES libmypaint)
find_library(LibMyPaint_LIBRARY NAMES mypaint libmypaint)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibMyPaint
    REQUIRED_VARS LibMyPaint_LIBRARY LibMyPaint_INCLUDE_DIR)

if(LibMyPaint_FOUND AND NOT TARGET LibMyPaint::LibMyPaint)
    add_library(LibMyPaint::LibMyPaint UNKNOWN IMPORTED)
    set_target_properties(LibMyPaint::LibMyPaint PROPERTIES
        IMPORTED_LOCATION "${LibMyPaint_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibMyPaint_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES json-c::json-c)
endif()

mark_as_advanced(LibMyPaint_INCLUDE_DIR LibMyPaint_LIBRARY)
