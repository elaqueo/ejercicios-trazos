#include "Brush.h"

#include <mypaint-brush.h>

namespace paintcore::detail {

Brush::Brush()
    : m_brush(mypaint_brush_new())
{
    mypaint_brush_from_defaults(m_brush);
}

Brush::~Brush()
{
    mypaint_brush_unref(m_brush);
}

} // namespace paintcore::detail
