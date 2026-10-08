# Ejercicios de trazos

App de escritorio para practicar control de trazo con tableta digitalizadora: genera ejercicios al azar, uno por pantalla, y con → pasa al siguiente. C++20, Qt 6 Widgets y libmypaint.

Es la primera app de una familia: `libs/paintcore` (motor de pinceles) y `libs/appkit` (framework de app) se comparten entre apps.

- [Alcance de la v1](docs/alcance-v1.md)
- [Requerimientos y backlog](docs/requerimientos-backlog.md)

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

## Compilar

```powershell
pwsh scripts/build.ps1                  # Debug
pwsh scripts/build.ps1 -Preset release
```

El ejecutable queda en `build/<preset>/ejercicios.exe`, con Qt desplegado al lado.

## Licencia

[MIT](LICENSE). Qt se usa bajo LGPLv3 con enlace dinámico.
