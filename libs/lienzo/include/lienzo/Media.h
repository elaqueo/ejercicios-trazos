#pragma once

#include <drymedia/Medium.h>

#include <QByteArray>
#include <QString>

#include <array>
#include <cstdint>

namespace lienzo {

// Parámetros calibrables de una mina (HU-57): lo que se ajusta en vivo y se guarda en
// medios.json.
struct Lead {
    int softness = 20;   // blandura, 1..255
    int diameter = 87;   // diámetro en centésimas de mm, 30..200
    int ceiling = 65535; // techo de tono (HU-56), 2000..65535
    // Forma de la punta (HU-59), igual para todas las minas: semiángulo del cono afilado
    // (5..30°) y la altitud de la máxima inclinación de la tableta (10..80°), que se toma
    // como el lápiz acostado. En medios.json van una sola vez, en "punta".
    int halfAngle = 12;
    int minAltitude = 30;

    bool operator==(const Lead&) const = default;
    Lead clamped() const;
    drymedia::Medium medium() const;

    // En 64 bits, para pasarla al hilo de simulación con un solo atómico (los valores
    // cambian juntos al elegir otra mina).
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

// La goma (HU-58), calibrable como las minas.
struct Eraser {
    int strength = 20;  // fuerza (cuánto quita por distancia), 1..255
    int diameter = 500; // diámetro en centésimas de mm, 200..800

    bool operator==(const Eraser&) const = default;
    Eraser clamped() const;
    drymedia::Medium medium() const;
    uint64_t pack() const;
    static Eraser unpack(uint64_t packed);
};

// Todo lo que guarda medios.json.
struct MediaSet {
    Grades grades = factoryGrades();
    Eraser eraser;
    bool operator==(const MediaSet&) const = default;
};

// medios.json. Lo que falte en el archivo queda con el valor de fábrica y los valores
// fuera de rango se recortan. Si el archivo no es JSON válido devuelve false, deja
// `media` sin tocar y describe el problema en `error`.
bool mediaFromJson(const QByteArray& json, MediaSet& media, QString* error = nullptr);
QByteArray mediaToJson(const MediaSet& media);

} // namespace lienzo
