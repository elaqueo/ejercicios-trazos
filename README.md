# Ejercicios de trazos

Apps de escritorio para dibujar con tableta digitalizadora sobre un lienzo propio de baja latencia, con grafito simulado (minas 2H a 6B y goma) sobre una hoja A4 a escala real. C++20, Qt 6 y Direct3D 11.

- **Ejercicios** (`apps/ejercicios`): genera ejercicios de trazo al azar, uno por hoja; → pasa al siguiente y R repite el mismo.
- **Cartuchera** (`apps/cartuchera`): hoja libre para dibujar, con deshacer.

- [Alcance de la v1](docs/alcance-v1.md)
- [Diseño (sistema visual y pantallas)](docs/diseno.md)
- [Requerimientos y backlog](docs/requerimientos-backlog.md) · [issues](https://github.com/elaqueo/ejercicios-trazos/issues) (una por historia) · [sprints](https://github.com/elaqueo/ejercicios-trazos/milestones) · [tablero](https://github.com/users/elaqueo/projects/3)

## Estructura

```text
libs/drymedia/     motor de medios secos: papel en tiles, punta, depósito y goma (sin Qt)
libs/tabletinput/  entrada del lápiz por WM_POINTER (Win32 puro)
libs/lienzo/       lienzo de baja latencia: ventana nativa con swapchain, hilos de simulación y render, minas
libs/appkit/       área útil, calibración, configuración, log, atajos, guías y tema
apps/ejercicios/   generadores de ejercicios y sesión
apps/cartuchera/   hoja libre
spikes/            mediciones de la Fase 0 de Cartuchera (se compilan para que no se rompan)
cmake/             opciones comunes de los targets
scripts/           build.ps1 (comando único) y vsenv.ps1 (entorno de MSVC)
```

## Requisitos (Windows)

- VS 2022 Build Tools con MSVC v143 (incluyen CMake ≥ 3.25 y Ninja).
- Qt 6.11.2 `msvc2022_64` en `%USERPROFILE%\Qt\6.11.2\msvc2022_64` (otra ubicación: `-DET_QT_ROOT=...`).

No hay otras dependencias: el motor de dibujo es propio. Hasta HU-68 se usaba libmypaint por vcpkg; ese código queda en el historial de git.

## Datos

Todas las apps de la familia comparten `%LOCALAPPDATA%\trazos\`: `config.json` (área útil por monitor, monitor elegido), `medios.json` (minas y goma calibradas; se recarga en caliente y Ctrl+S guarda), y un `<app>.log` que se reescribe en cada arranque.

## Compilar

```powershell
pwsh scripts/build.ps1                  # Debug
pwsh scripts/build.ps1 -Preset release
```

El comando configura, compila y corre las pruebas. Los ejecutables quedan en `build/<preset>/` (`ejercicios.exe`, `cartuchera.exe`), con Qt desplegado al lado. Después de cambiar los presets, `-Fresh` reconfigura desde cero.

Las apps abren a pantalla completa en el monitor guardado (F10 pasa al siguiente y lo recuerda; Alt+F4 sale). Las teclas están en el registro único de atajos de cada app (`lienzo/Lienzo.h` y el `main.cpp` de cada una).

**Área útil:** F9 la calibra tocando con el lápiz la esquina superior izquierda y la inferior derecha de la superficie activa de la tableta. La hoja se ubica dentro de ese rectángulo, a escala de la tableta. Se guarda por monitor.

## Pruebas

Qt Test + CTest. Cada biblioteca o app tiene su carpeta `tests/`; un test nuevo se declara con `et_add_qt_test(tst_nombre SOURCES tst_nombre.cpp LIBS <target>)`. Los tests corren con la plataforma `offscreen` (sin abrir ventanas).

```powershell
pwsh scripts/build.ps1                  # también corre las pruebas
ctest --preset debug                    # solo las pruebas (con MSVC cargado)
```

## Licencia

[MIT](LICENSE). Qt se usa bajo LGPLv3 con enlace dinámico.
