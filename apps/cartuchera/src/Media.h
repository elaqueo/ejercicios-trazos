#pragma once

#include <drymedia/Medium.h>

#include <QByteArray>
#include <QString>

#include <array>
#include <cstdint>

namespace cartuchera {

// Parámetros calibrables de una mina (HU-57): lo que se ajusta en vivo y se guarda en
// medios.json.
struct Lead {
    int softness = 20;   // blandura, 1..255
    int diameter = 87;   // diámetro en centésimas de mm, 30..200
    int ceiling = 65535; // techo de tono (HU-56), 2000..65535

    bool operator==(const Lead&) const = default;
    Lead clamped() const;
    drymedia::Medium medium() const;

    // En 64 bits, para pasarla al hilo de simulación con un solo atómico (los tres
    // valores cambian juntos al elegir otra mina).
    uint64_t pack() const;
    static Lead unpack(uint64_t packed);
};

// Durezas en el orden de las teclas 1 a 0.
constexpr int kGradeCount = 10;
inline constexpr std::array<const char*, kGradeCount> kGradeNames{"2H", "H",  "F",  "HB", "B",
                                                                  "2B", "3B", "4B", "5B", "6B"};
constexpr int kHbIndex = 3;
using Grades = std::array<Lead, kGradeCount>;

// Valores de fábrica: punto de partida para calibrar. La HB es la calibrada en HU-51.
Grades factoryGrades();

// medios.json. Lo que falte en el archivo queda con el valor de fábrica y los valores
// fuera de rango se recortan. Si el archivo no es JSON válido devuelve false, deja
// `grades` sin tocar y describe el problema en `error`.
bool gradesFromJson(const QByteArray& json, Grades& grades, QString* error = nullptr);
QByteArray gradesToJson(const Grades& grades);

} // namespace cartuchera
