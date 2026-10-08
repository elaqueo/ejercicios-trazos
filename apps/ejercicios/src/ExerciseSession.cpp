#include "ExerciseSession.h"

#include <paintcore/CanvasWidget.h>

#include <QRandomGenerator>

namespace ejercicios {

namespace {

quint32 newSeed(quint32 previous)
{
    quint32 seed;
    do {
        seed = QRandomGenerator::global()->generate();
    } while (seed == previous);
    return seed;
}

} // namespace

ExerciseSession::ExerciseSession(paintcore::CanvasWidget* canvas, const Exercise* exercise, QObject* parent)
    : QObject(parent)
    , m_canvas(canvas)
    , m_exercise(exercise)
    , m_seed(newSeed(0))
{
}

void ExerciseSession::next()
{
    m_seed = newSeed(m_seed);
    m_canvas->clear();
    regenerate();
}

void ExerciseSession::regenerate()
{
    // Coordenadas del lienzo: el origen es la esquina del área útil.
    const QRect area(QPoint(0, 0), m_canvas->canvasRect().size());
    const SafeZone zone = SafeZone::withRandomOrientation(area, m_seed);
    m_current = m_exercise->generate(m_exercise->defaults(), m_seed, zone);
    m_canvas->setGuides(m_current.guides);
}

} // namespace ejercicios
