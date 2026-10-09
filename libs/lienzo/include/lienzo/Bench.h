#pragma once

#include "lienzo/SheetMapping.h"

namespace lienzo {

class SampleQueue;
class Simulation;

// Benchmark sintético (--bench [undo] [costado] en Cartuchera): durante 30 s inyecta
// trazos a 133 muestras/s (lo que entrega la tableta, HU-43) como si fueran del lápiz, con
// deshacer y rehacer cada pocos trazos si withUndo. Con side, el peor caso del sombreado
// (HU-59): la 6B acostada, con el azimut girando. Al terminar cierra la app; los tiempos
// quedan en el log. Sirve para medir la latencia sin depender de la mano.
void runBench(SampleQueue& queue, Simulation& sim, const SheetMapping& mapping, bool withUndo, bool side = false);

} // namespace lienzo
