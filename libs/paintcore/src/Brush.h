#pragma once

// Header privado de paintcore: es el único lugar, junto con su .cpp, que conoce
// los tipos de libmypaint. No se instala ni se incluye desde include/ (RNF-05).

#include <QByteArray>

struct MyPaintBrush;

namespace paintcore::detail {

// Dueño de un MyPaintBrush (RAII). Arranca con los valores por defecto de libmypaint.
class Brush {
public:
    Brush();
    ~Brush();

    Brush(const Brush&) = delete;
    Brush& operator=(const Brush&) = delete;

    MyPaintBrush* handle() const { return m_brush; }

    // true si libmypaint acepta el JSON como pincel (.myb versión 3).
    static bool isValidJson(const QByteArray& json);

private:
    MyPaintBrush* m_brush = nullptr;
};

} // namespace paintcore::detail
