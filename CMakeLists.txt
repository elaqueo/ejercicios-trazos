cmake_minimum_required(VERSION 3.25)

project(EjerciciosTrazos VERSION 0.1.0 LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "Este proyecto requiere MSVC: los binarios de Qt para Windows usan la ABI de MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- CRT y Qt Release en todas las configuraciones ---------------------------
# Debug compila NUESTRO código sin optimizar y con símbolos, pero con CRT /MD y
# consumiendo la variante Release de los targets importados (Qt y, desde HU-02,
# las dependencias de vcpkg, que se compilan solo en Release). Así hay un único
# CRT y un único Qt en el proceso, igual que en anim-sandbox.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG "Release;")

# --- Qt ----------------------------------------------------------------------
# Qt instalado con aqtinstall en <ET_QT_ROOT>/<versión>/msvc2022_64 (compartido
# con anim-sandbox). Se enlaza dinámicamente, como exige la LGPLv3.
set(ET_QT_ROOT "$ENV{USERPROFILE}/Qt" CACHE PATH "Carpeta raíz de las instalaciones de Qt (aqtinstall)")
set(ET_QT_VERSION "6.11.2" CACHE STRING "Versión de Qt a usar")
file(TO_CMAKE_PATH "${ET_QT_ROOT}/${ET_QT_VERSION}/msvc2022_64" ET_QT_PREFIX)
list(PREPEND CMAKE_PREFIX_PATH "${ET_QT_PREFIX}")

find_package(Qt6 ${ET_QT_VERSION} EXACT REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EtTargetDefaults)

# libmypaint (vcpkg). Se busca acá y no en libs/paintcore porque los targets
# importados tienen alcance de directorio: las apps enlazan paintcore estática y
# necesitan ver LibMyPaint::LibMyPaint. Solo paintcore la enlaza (PRIVATE).
find_package(LibMyPaint REQUIRED)

add_subdirectory(libs/paintcore)
add_subdirectory(libs/appkit)
add_subdirectory(apps/ejercicios)
# Spikes de Cartuchera (Fase 0): código descartable que se compila igual, para que no
# se rompa sin que nadie lo note.
add_subdirectory(spikes/cartuchera-entrada)
add_subdirectory(spikes/cartuchera-swapchain)
