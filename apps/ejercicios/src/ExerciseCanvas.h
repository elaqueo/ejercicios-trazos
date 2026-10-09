#pragma once

#include <QPicture>
#include <QSize>

namespace ejercicios {

// Lo que la sesión necesita del lienzo (HU-65): borrar, el tamaño de la hoja en píxeles y
// las guías (en coordenadas de la hoja: el origen es su esquina). En la app lo implementa
// el lienzo de baja latencia (lienzo::Lienzo); en los tests, uno de mentira.
class ExerciseCanvas {
public:
    virtual ~ExerciseCanvas() = default;
    virtual void clear() = 0;
    virtual QSize sheetSize() const = 0;
    virtual void setGuides(const QPicture& guides) = 0;
};

} // namespace ejercicios
