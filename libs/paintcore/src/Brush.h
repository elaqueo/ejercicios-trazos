#pragma once

// Header privado de paintcore: es el único lugar, junto con su .cpp, que conoce
// los tipos de libmypaint. No se instala ni se incluye desde include/ (RNF-05).

#include <QByteArray>

struct MyPaintBrush;

namespace paintcore::detail {

// Dueño de un MyPaintBrush (RAII). Arranca con el pincel por defecto.
class Brush {
public:
    Brush();
    ~Brush();

    Brush(const Brush&) = delete;
    Brush& operator=(const Brush&) = delete;

    MyPaintBrush* handle() const { return m_brush; }

    // Pincel por defecto: una birome negra con grosor y opacidad según la presión.
    void loadDefault();

    // Carga un .myb (JSON). Si libmypaint lo rechaza, queda el pincel por defecto
    // y devuelve false. En los dos casos la tinta queda negra.
    bool loadJson(const QByteArray& json);

    // true si libmypaint acepta el JSON como pincel (.myb versión 3).
    static bool isValidJson(const QByteArray& json);

    // Píxeles de superficie por píxel de pantalla (supersampling). El radio base se
    // multiplica por scale, en este pincel y en los que se carguen después, para que
    // el trazo tenga en pantalla el grosor que define el .myb.
    void setPixelScale(float scale);

private:
    void forceBlackInk();
    void applyPixelScale(float logFactor);

    MyPaintBrush* m_brush = nullptr;
    float m_logScale = 0.0f; // ln(scale)
};

} // namespace paintcore::detail
