# Ejercicios de trazos

App de escritorio para practicar control de trazo con tableta digitalizadora: genera ejercicios al azar, uno por pantalla, y con → pasa al siguiente. C++20, Qt 6 Widgets y libmypaint.

Es la primera app de una familia: `libs/paintcore` (motor de pinceles) y `libs/appkit` (framework de app) se comparten entre apps.

- [Alcance de la v1](docs/alcance-v1.md)
- [Requerimientos y backlog](docs/requerimientos-backlog.md) · [issues](https://github.com/elaqueo/ejercicios-trazos/issues) (una por historia) · [sprints](https://github.com/elaqueo/ejercicios-trazos/milestones) · [tablero](https://github.com/users/elaqueo/projects/3)

## Estructura

```text
libs/paintcore/   wrapper de libmypaint, superficie, input, lienzo (no depende de appkit)
libs/appkit/      ventana, área útil, overlays, configuración, atajos
apps/ejercicios/  generadores de ejercicios y lógica propia de esta app
cmake/            opciones comunes de los targets
scripts/          build.ps1 (comando único) y vsenv.ps1 (entorno de MSVC)
```

## Requisitos (Windows)

- VS 2022 Build Tools con MSVC v143 (incluyen CMake ≥ 3.25 y Ninja).
- Qt 6.11.2 `msvc2022_64` en `%USERPROFILE%\Qt\6.11.2\msvc2022_64` (otra ubicación: `-DET_QT_ROOT=...`).
- [vcpkg](https://github.com/microsoft/vcpkg) con la variable `VCPKG_ROOT` apuntando a su carpeta.

## Dependencias (vcpkg)

`vcpkg.json` declara libmypaint y mypaint-brushes en modo manifiesto, con el `builtin-baseline` fijado. El primer configure las compila (y baja las herramientas de MSYS2 que vcpkg usa para autotools); después salen del caché binario de vcpkg.

- `vcpkg/ports/libmypaint/`: overlay del port oficial que compila **sin glib** (solo hace falta para GEGL e introspection, y nos evita arrastrar glib, gettext, pcre2 y libffi) y con `fix_unknown_ids_msvc.patch`, que evita una corrupción del heap con MSVC al cargar `.myb` con ajustes que libmypaint 1.6 no conoce.
- `vcpkg/triplets/x64-windows-static-md-rel.cmake`: bibliotecas estáticas con CRT `/MD`, solo Release.
- Solo `paintcore` ve los headers de libmypaint; el resto del código no sabe que existe.

## Pinceles y datos

Los pinceles `.myb` (formato JSON de MyPaint 2.x, también usado por Krita) se cargan de dos carpetas, en orden:

1. `build/<preset>/brushes/`: los de fábrica, del port `mypaint-brushes` (CC0), copiados por el build.
2. `%LOCALAPPDATA%\trazos\brushes\`: carpeta compartida por todas las apps de la familia, para packs propios. Un pincel con el mismo nombre reemplaza al de fábrica.

Los `.myb` inválidos se ignoran y quedan registrados en `%LOCALAPPDATA%\trazos\ejercicios.log`, que se reescribe en cada arranque.

## Compilar

```powershell
pwsh scripts/build.ps1                  # Debug
pwsh scripts/build.ps1 -Preset release
```

El comando configura, compila y corre las pruebas. El ejecutable queda en `build/<preset>/ejercicios.exe`, con Qt desplegado al lado.

La app abre a pantalla completa en el monitor guardado (F10 pasa al siguiente y lo recuerda; Alt+F4 sale). Para desarrollar, `ejercicios.exe --ventana` abre en una ventana común.

**Área útil:** F9 la calibra tocando con el lápiz la esquina superior izquierda y la inferior derecha de la superficie activa de la tableta. El lienzo ocupa ese rectángulo y fuera se ve gris. Se guarda por monitor.

## Pruebas

Qt Test + CTest. Cada biblioteca o app tiene su carpeta `tests/`; un test nuevo se declara con `et_add_qt_test(tst_nombre SOURCES tst_nombre.cpp LIBS <target>)`. Los tests corren con la plataforma `offscreen` (sin abrir ventanas).

```powershell
pwsh scripts/build.ps1                  # también corre las pruebas
ctest --preset debug                    # solo las pruebas (con MSVC cargado)
```

## Licencia

[MIT](LICENSE). Qt se usa bajo LGPLv3 con enlace dinámico.
