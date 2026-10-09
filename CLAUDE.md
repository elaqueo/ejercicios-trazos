# Ejercicios de trazos · guía para sesiones de Claude

- Monorepo de apps de tableta: `apps/ejercicios` y `apps/cartuchera`, las dos sobre `libs/lienzo` (lienzo de baja latencia: ventana nativa con swapchain, motor `libs/drymedia`, entrada `libs/tabletinput`); `libs/appkit` (área útil, calibración, config, log, atajos, guías, tema). Sin dependencias fuera de Qt: libmypaint, paintcore y vcpkg salieron en HU-68 (quedan en el historial de git). Reglas de alcance en `docs/alcance-v1.md`; backlog en `docs/requerimientos-backlog.md` e issues (#N = HU-N); diseño en `docs/diseno.md`.
- **Motor de medios secos** (`drymedia`, `tabletinput`, `lienzo`): leer `docs/medios-secos/arquitectura.md` entero antes de tocarlo. La entrada va por WM_POINTER directo, no por QTabletEvent.
- Instalador: `pwsh scripts/package.ps1` → `dist/TrazosSetup-<versión>.exe` (Inno Setup; versión en `project()` del CMakeLists raíz).
- Build y tests con un solo comando: `pwsh scripts/build.ps1` (Debug) o `-Preset release`. Cerrar la app antes de compilar (el exe queda bloqueado).
- Forma de trabajo: una historia por vez. Proponer, esperar OK, implementar con tests, verificar (build + tests + abrir la app), prueba del usuario con la tableta, commit con `Closes #N` y push, informe corto, pedir OK para la siguiente.
- Explicar el porqué de cada decisión, en español rioplatense, breve.
