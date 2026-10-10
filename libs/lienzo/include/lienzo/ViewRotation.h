#pragma once

#include <cmath>
#include <numbers>

namespace lienzo {

// Rotación de la vista (HU-40): la imagen de pantalla se muestra girada `degrees` alrededor
// de (cx, cy), en píxeles del cliente. Con y hacia abajo, los grados positivos giran en
// sentido horario. El render dibuja la imagen así; la simulación lleva cada muestra del
// lápiz de vuelta a la imagen sin rotar, así el trazo cae bajo la punta.
struct ViewRotation {
    double degrees = 0;
    double cx = 0, cy = 0;

    static constexpr double kSnapStep = 15.0; // como el Ejercicios anterior (HU-07)

    bool identity() const { return degrees == 0; }

    // Pantalla → imagen sin rotar.
    void toImage(double& x, double& y) const
    {
        if (identity())
            return;
        const double a = -degrees * std::numbers::pi / 180.0;
        rotate(a, x, y);
    }

    // Imagen sin rotar → pantalla.
    void toScreen(double& x, double& y) const
    {
        if (identity())
            return;
        const double a = degrees * std::numbers::pi / 180.0;
        rotate(a, x, y);
    }

    // Ángulo en (−180, 180].
    static double normalized(double degrees)
    {
        double a = std::fmod(degrees, 360.0);
        if (a <= -180.0)
            a += 360.0;
        if (a > 180.0)
            a -= 360.0;
        return a;
    }

    static double snapped(double degrees) { return std::round(degrees / kSnapStep) * kSnapStep; }

    // Shift + arrastrar (HU-79): la vista gira lo mismo que recorre la punta alrededor del
    // centro, libre, sin snap. Los ángulos de la punta, en grados.
    static double dragged(double startDegrees, double startPointer, double pointer)
    {
        return normalized(startDegrees + pointer - startPointer);
    }

private:
    void rotate(double radians, double& x, double& y) const
    {
        const double c = std::cos(radians), s = std::sin(radians);
        const double dx = x - cx, dy = y - cy;
        x = cx + c * dx - s * dy;
        y = cy + s * dx + c * dy;
    }
};

} // namespace lienzo
