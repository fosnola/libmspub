/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ColorReference.h"

namespace libmspub
{

Color ColorReference::getRealColor(unsigned c, const std::vector<Color> &palette) const
{
  unsigned char type = static_cast<unsigned char>((c >> 24) & 0xFF);
  if (type == 0x08)
  {
    if ((c & 0xFFFFFF) >= palette.size())
    {
      return Color();
    }
    return palette[c & 0xFFFFFF];
  }
  return Color(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF);
}
Color ColorReference::getFinalColor(const std::vector<Color> &palette) const
{
  unsigned char modifiedType = static_cast<unsigned char>((m_modifiedColor >> 24) & 0xFF);
  if (modifiedType == CHANGE_INTENSITY)
  {
    Color c = getRealColor(m_baseColor, palette);
    unsigned char changeIntensityBase = (m_modifiedColor >> 8) & 0xFF;
    double intensity = double((m_modifiedColor >> 16) & 0xFF) / 0xFF;
    if (changeIntensityBase == BLACK_BASE)
    {
      return Color(static_cast<unsigned char>(c.r * intensity), static_cast<unsigned char>(c.g * intensity), static_cast<unsigned char>(c.b * intensity));
    }
    if (changeIntensityBase == WHITE_BASE)
    {
      return Color(static_cast<unsigned char>(c.r + (255 - c.r) * (1 - intensity)), static_cast<unsigned char>(c.g + (255 - c.g) * (1 - intensity)), static_cast<unsigned char>(c.b + (255 - c.b) * (1 - intensity)));
    }
    return Color();
  }
  else
  {
    return getRealColor(m_modifiedColor, palette);
  }
}

bool operator==(const ColorReference &l, const ColorReference &r)
{
  return l.m_baseColor == r.m_baseColor && l.m_modifiedColor == r.m_modifiedColor;
}

// const unsigned char ColorReference::COLOR_PALETTE = 0x8;

const unsigned char ColorReference::CHANGE_INTENSITY = 0x10;

const unsigned char ColorReference::BLACK_BASE = 0x1;

const unsigned char ColorReference::WHITE_BASE = 0x2;

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
