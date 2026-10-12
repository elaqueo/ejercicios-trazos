#include "exercises/StrokeStacks.h"

#include "exercises/Cajas.h"
#include "exercises/CajasRotadas.h"
#include "exercises/Concentricas.h"
#include "exercises/Curva.h"
#include "exercises/CurvaDireccion.h"
#include "exercises/Direccion.h"
#include "exercises/Elipse.h"
#include "exercises/ElipsePerspectiva.h"
#include "exercises/Hatching.h"
#include "exercises/Radiales.h"
#include "exercises/Recta.h"

namespace ejercicios {

StrokeStacks::StrokeStacks()
{
    m_exercises.push_back(std::make_unique<Recta>());
    m_exercises.push_back(std::make_unique<Hatching>());
    m_exercises.push_back(std::make_unique<Radiales>());
    m_exercises.push_back(std::make_unique<Direccion>());
    m_exercises.push_back(std::make_unique<Curva>());
    m_exercises.push_back(std::make_unique<CurvaDireccion>());
    m_exercises.push_back(std::make_unique<Elipse>());
    m_exercises.push_back(std::make_unique<Concentricas>());
    m_exercises.push_back(std::make_unique<ElipsePerspectiva>());
    m_exercises.push_back(std::make_unique<Cajas>());
    m_exercises.push_back(std::make_unique<CajasRotadas>());
    for (const auto& exercise : m_exercises) {
        m_stacks.push_back(std::make_unique<SameOnEverySheet>(*exercise, kStrokeSheets));
        m_list.append(m_stacks.back().get());
    }
}

StrokeStacks::~StrokeStacks() = default;

} // namespace ejercicios
