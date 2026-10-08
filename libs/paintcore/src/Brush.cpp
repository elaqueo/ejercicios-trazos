#include "Brush.h"

#include <mypaint-brush.h>

namespace paintcore::detail {

Brush::Brush()
    : m_brush(mypaint_brush_new())
{
    // Pincel por defecto hasta que lleguen los .myb (HU-05): una birome negra.
    // from_defaults ya hace opacidad ∝ presión; le sumamos grosor ∝ presión.
    mypaint_brush_from_defaults(m_brush);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, 1.2f); // ~3,3 px
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_HARDNESS, 0.9f);
    mypaint_brush_set_base_value(m_brush, MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, 4.0f);
    mypaint_brush_set_mapping_n(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, MYPAINT_BRUSH_INPUT_PRESSURE, 2);
    mypaint_brush_set_mapping_point(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, MYPAINT_BRUSH_INPUT_PRESSURE, 0, 0.0f, -0.8f);
    mypaint_brush_set_mapping_point(m_brush, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, MYPAINT_BRUSH_INPUT_PRESSURE, 1, 1.0f, 0.6f);
}

Brush::~Brush()
{
    mypaint_brush_unref(m_brush);
}

} // namespace paintcore::detail
