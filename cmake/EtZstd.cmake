# zstd (HU-85): compresión sin pérdida de las hojas de la toma (spike en
# docs/mesa-de-animacion/spikes/toma.md). Licencia BSD. Se baja la fuente de la versión fija
# (con su hash) y se compila estática con el CRT del proyecto: no hay DLL que desplegar.
# Solo compresión y descompresión: sin programas, sin formatos viejos, sin hilos.
include(FetchContent)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
# Su CMakeLists pide una versión vieja de CMake: sin esto ignora CMAKE_MSVC_RUNTIME_LIBRARY y
# en Debug compila con el CRT de depuración.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
FetchContent_Declare(zstd
    URL https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz
    URL_HASH SHA256=8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1
    SOURCE_SUBDIR build/cmake)
FetchContent_MakeAvailable(zstd)
add_library(et_zstd INTERFACE)
target_link_libraries(et_zstd INTERFACE libzstd_static)
target_include_directories(et_zstd SYSTEM INTERFACE "${zstd_SOURCE_DIR}/lib")
