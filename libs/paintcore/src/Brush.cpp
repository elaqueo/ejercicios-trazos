#include "Brush.h"

#include <mypaint-brush.h>

#include <cmath>

namespace paintcore::detail {

Brush::Brush()
    : m_brush(mypaint_brush_new())
{
    loadDefault();
}

Brush::~Brush()
{
    mypaint_brush_unref(m_brush);
}

void Brush::loadDefault()
{
    // from_defaults ya hace opacidad ∝ presión; le sumamos grosor ∝ presión.
    mypaint_brush_from_defaults(m_brush);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.2f); // ~3,3 px
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_HARDNESS, 0.9f);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
    mypaint_brush_set_mapping_n(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, MYPAINT_BRUSH_INPUT_PRESSURE, 2);
    mypaint_brush_set_mapping_point(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, MYPAINT_BRUSH_INPUT_PRESSURE, 0, 0.0f, -0.8f);
    mypaint_brush_set_mapping_point(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, MYPAINT_BRUSH_INPUT_PRESSURE, 1, 1.0f, 0.6f);
    forceBlackInk();
    applyPixelScale(m_logScale);
}

bool Brush::loadJson(const QByteArray& json)
{
    // from_string solo pisa los ajustes que trae el archivo: se parte de los valores
    // por defecto de libmypaint para que no queden restos del pincel anterior.
    mypaint_brush_from_defaults(m_brush);
    if (!mypaint_brush_from_string(m_brush, json.constData())) {
        loadDefault();
        return false;
    }
    forceBlackInk();
    applyPixelScale(m_logScale);
    return true;
}

void Brush::setPixelScale(float scale)
{
    const float logScale = std::log(scale);
    applyPixelScale(logScale - m_logScale);
    m_logScale = logScale;
}

void Brush::applyPixelScale(float logFactor)
{
    // El radio es logarítmico: multiplicarlo por scale es sumar ln(scale) a la base
    // (las entradas como la presión se suman encima, así que escalan igual).
    const float base = mypaint_brush_get_base_value(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, base + logFactor);
}

void Brush::forceBlackInk()
{
    // Los .myb traen su propio color y dinámicas de color; para practicar trazo
    // la tinta es siempre negra (los colores llegan con el panel de configuración).
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_COLOR_H, 0.0f);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_COLOR_S, 0.0f);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_COLOR_V, 0.0f);
    const MyPaintBrushSetting dynamics[] = {
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_H, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_L,
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSL_S, MYPAINT_BRUSH_SETTING_CHANGE_COLOR_V,
        MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSV_S, MYPAINT_BRUSH_SETTING_COLORIZE,
    };
    for (const MyPaintBrushSetting setting : dynamics) {
        mypaint_brush_set_base_value(m_brush, setting, 0.0f);
        for (int input = 0; input < MYPAINT_BRUSH_INPUTS_COUNT; ++input)
            mypaint_brush_set_mapping_n(m_brush, setting, static_cast<MyPaintBrushInput>(input), 0);
    }
}

bool Brush::isValidJson(const QByteArray& json)
{
    // libmypaint escribe el motivo del rechazo en stderr; el llamador lo registra en el log.
    MyPaintBrush* probe = mypaint_brush_new();
    const bool valid = mypaint_brush_from_string(probe, json.constData());
    mypaint_brush_unref(probe);
    return valid;
}

} // namespace paintcore::detail
