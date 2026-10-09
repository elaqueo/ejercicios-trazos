#include "ExerciseSession.h"

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

ExerciseSession::ExerciseSession(ExerciseCanvas* canvas, const Exercise* exercise, QObject* parent)
    : QObject(parent)
    , m_canvas(canvas)
    , m_exercise(exercise)
    , m_seed(newSeed(0))
    , m_currentParams(params(exercise))
{
}

QVariantMap ExerciseSession::params(const Exercise* exercise) const
{
    return appkit::clampValues(exercise->params(), m_configured.value(exercise->id(), exercise->defaults()));
}

void ExerciseSession::setParams(const QVariantMap& values)
{
    m_configured.insert(m_exercise->id(), appkit::clampValues(m_exercise->params(), values));
}

void ExerciseSession::next()
{
    m_seed = newSeed(m_seed);
    m_currentParams = params(m_exercise);
    m_canvas->clear();
    regenerate();
}

void ExerciseSession::setExercise(const Exercise* exercise)
{
    m_exercise = exercise;
    next();
}

void ExerciseSession::repeat()
{
    m_canvas->clear();
    regenerate();
}

void ExerciseSession::regenerate()
{
    // Coordenadas de la hoja: el origen es su esquina.
    const QRect area(QPoint(0, 0), m_canvas->sheetSize());
    const SafeZone zone = SafeZone::withRandomOrientation(area, m_seed);
    m_current = m_exercise->generate(m_currentParams, m_seed, zone);
    m_canvas->setGuides(m_current.guides);
}

} // namespace ejercicios
