# Spike HU-85 (Mesa de animación): qué compresión sin pérdida usar para las hojas guardadas
# en disco (zlib de Qt, zstd, lz4). Consola. Medir con el preset release. Código descartable.
# Opcional (ET_SPIKE_TOMA): baja lz4 con FetchContent, que el proyecto no usa.
include(FetchContent)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
FetchContent_Declare(lz4 URL https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
                     SOURCE_SUBDIR build/cmake)
FetchContent_MakeAvailable(lz4) # zstd ya está en el proyecto (cmake/EtZstd.cmake)

qt_add_executable(mesa-toma main.cpp)
target_include_directories(mesa-toma PRIVATE "${lz4_SOURCE_DIR}/lib")
target_link_libraries(mesa-toma PRIVATE lienzo drymedia et_zstd lz4_static)
et_target_defaults(mesa-toma)
set_target_properties(mesa-toma PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
