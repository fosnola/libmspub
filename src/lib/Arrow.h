/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_ARROW_H
#define INCLUDED_ARROW_H

#include <librevenge/librevenge.h>

namespace libmspub
{
enum ArrowStyle
{
  NO_ARROW,
  TRIANGLE_ARROW,
  STEALTH_ANGLE_ARROW,
  ROTATED_SQUARE_ARROW,
  CIRCLE_ARROW,
  LINE_ARROW
};
enum ArrowSize
{
  SMALL,
  MEDIUM,
  LARGE
};
struct Arrow
{
  ArrowStyle m_style;
  ArrowSize m_width;
  ArrowSize m_height;
  Arrow(ArrowStyle style, ArrowSize width, ArrowSize height) :
    m_style(style), m_width(width), m_height(height)
  {
  }
  void addTo(librevenge::RVNGPropertyList &propList, bool start) const;
};
} // namespace libmspub

#endif /* INCLUDED_ARROW_H */
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
