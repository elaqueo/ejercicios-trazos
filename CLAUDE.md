# Ejercicios de trazos · guía para sesiones de Claude

- Monorepo de apps de tableta: `libs/paintcore` (libmypaint, sin depender de appkit), `libs/appkit` (ventana, área útil, overlays, config), `apps/ejercicios`. Reglas de alcance en `docs/alcance-v1.md`; backlog en `docs/requerimientos-backlog.md` e issues (#N = HU-N); diseño en `docs/diseno.md`.
- **Cartuchera** (app de medios secos, libs `drymedia` y `tabletinput`): leer `docs/medios-secos/arquitectura.md` entero antes de tocar algo de ese proyecto. Motor propio sin libmypaint; la entrada va por WM_POINTER directo, no por QTabletEvent.
- Build y tests con un solo comando: `pwsh scripts/build.ps1` (Debug) o `-Preset release`. Cerrar la app antes de compilar (el exe queda bloqueado).
- Forma de trabajo: una historia por vez. Proponer, esperar OK, implementar con tests, verificar (build + tests + abrir la app), prueba del usuario con la tableta, commit con `Closes #N` y push, informe corto, pedir OK para la siguiente.
- Explicar el porqué de cada decisión, en español rioplatense, breve.
