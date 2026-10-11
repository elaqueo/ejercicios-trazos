#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace lienzo {

// La vista (HU-40 giro, HU-101 zoom, HU-102 pan): la imagen de pantalla se muestra girada
// `degrees` y escalada `scale` alrededor de (cx, cy), y corrida (panX, panY), en píxeles del
// cliente: pantalla = c + pan + scale · R · (imagen − c). Con y hacia abajo, los grados
// positivos giran en sentido horario. El render dibuja la imagen así; la simulación lleva cada
// muestra del lápiz de vuelta a la imagen, así el trazo cae bajo la punta.
struct ViewRotation {
    double degrees = 0;
    double cx = 0, cy = 0;
    double scale = 1;
    double panX = 0, panY = 0;

    static constexpr double kSnapStep = 15.0; // como el Ejercicios anterior (HU-07)
    static constexpr double kMinScale = 0.25, kMaxScale = 4.0; // HU-101: de 25 % a 400 %
    static constexpr double kZoomStep = 1.2;                    // + y −, de a ~20 %

    bool identity() const { return degrees == 0 && scale == 1 && panX == 0 && panY == 0; }

    // Pantalla → imagen.
    void toImage(double& x, double& y) const
    {
        if (identity())
            return;
        const double a = -degrees * std::numbers::pi / 180.0;
        const double c = std::cos(a), s = std::sin(a);
        const double dx = (x - cx - panX) / scale, dy = (y - cy - panY) / scale;
        x = cx + c * dx - s * dy;
        y = cy + s * dx + c * dy;
    }

    // Imagen → pantalla.
    void toScreen(double& x, double& y) const
    {
        if (identity())
            return;
        const double a = degrees * std::numbers::pi / 180.0;
        const double c = std::cos(a), s = std::sin(a);
        const double dx = x - cx, dy = y - cy;
        x = cx + panX + scale * (c * dx - s * dy);
        y = cy + panY + scale * (s * dx + c * dy);
    }

    // HU-102: la misma vista con el corrimiento limitado para que el centro de la hoja no salga
    // del cliente (width × height): la hoja nunca se pierde de vista.
    ViewRotation clampedPan(double width, double height) const
    {
        ViewRotation v = *this;
        v.panX = std::clamp(panX, -cx, width - cx);
        v.panY = std::clamp(panY, -cy, height - cy);
        return v;
    }

    // HU-101: la misma vista con escala `to` (dentro de los límites), con el punto de pantalla
    // (x, y) quieto: lo que estaba bajo él sigue ahí (el zoom hacia la punta).
    ViewRotation zoomedAt(double to, double x, double y) const
    {
        double ix = x, iy = y;
        toImage(ix, iy);
        ViewRotation v = *this;
        v.scale = std::clamp(to, kMinScale, kMaxScale);
        v.panX = v.panY = 0;
        double sx = ix, sy = iy;
        v.toScreen(sx, sy);
        v.panX = x - sx;
        v.panY = y - sy;
        if (v.scale == 1 && std::abs(v.panX) < 1e-9 && std::abs(v.panY) < 1e-9)
            v.panX = v.panY = 0;
        return v;
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
};

} // namespace lienzo
