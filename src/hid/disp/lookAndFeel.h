#pragma once
#include <cmath>
#include "daisy_core.h"
#include "graphics_common.h"
#include "display.h"

#ifndef deg2rad
#define deg2rad(deg) ((deg)*3.141592 / 180.0)
#endif

namespace daisy
{
/** Implements drawing routines for menus, sliders and other common tasks. Overwrite the
 *  member functions and assign them to a UI object to create custom looking UIs.
 */
class OneBitGraphicsLookAndFeel
{
  public:
    virtual ~OneBitGraphicsLookAndFeel() {}
};
} // namespace daisy