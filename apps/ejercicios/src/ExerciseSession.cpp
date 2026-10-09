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

QVariantMap ExerciseSession::allParams() const
{
    QVariantMap all;
    for (auto it = m_configured.cbegin(); it != m_configured.cend(); ++it)
        all.insert(it.key(), it.value());
    return all;
}

void ExerciseSession::loadParams(const QVariantMap& all)
{
    // Se guardan tal cual (los de ejercicios que hoy no están se conservan); se recortan al
    // usarlos, en params().
    m_configured.clear();
    for (auto it = all.cbegin(); it != all.cend(); ++it)
        m_configured.insert(it.key(), it.value().toMap());
    m_currentParams = params(m_exercise);
}

const Exercise* ExerciseSession::pickMixed(const QList<const Exercise*>& pool, const QStringList& recent,
                                           QRandomGenerator& rng)
{
    if (pool.isEmpty())
        return nullptr;
    QList<const Exercise*> candidates = pool;
    if (recent.size() >= 2 && recent[recent.size() - 1] == recent[recent.size() - 2] && pool.size() > 1)
        candidates.removeIf([&](const Exercise* e) { return e->id() == recent.last(); });
    return candidates[int(rng.bounded(quint32(candidates.size())))];
}

void ExerciseSession::startMixed()
{
    m_mixed = true;
    next();
}

void ExerciseSession::next()
{
    if (m_mixed) {
        if (const Exercise* picked = pickMixed(m_pool, m_recent, *QRandomGenerator::global()))
            m_exercise = picked;
    }
    m_recent.append(m_exercise->id());
    while (m_recent.size() > 2)
        m_recent.removeFirst();
    m_seed = newSeed(m_seed);
    m_currentParams = params(m_exercise);
    m_canvas->clear();
    regenerate();
    if (onExerciseChanged)
        onExerciseChanged();
}

void ExerciseSession::setExercise(const Exercise* exercise)
{
    m_mixed = false;
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
    SafeZone zone = SafeZone::withRandomOrientation(area, m_seed);
    zone.pixelsPerMm = m_canvas->pixelsPerMm();
    m_current = m_exercise->generate(m_currentParams, m_seed, zone);
    m_canvas->setGuides(m_current.guides);
}

} // namespace ejercicios
