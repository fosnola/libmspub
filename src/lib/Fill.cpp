/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Fill.h"

#include <utility>

#include "FillType.h"
#include "MSPUBCollector.h"
#include "libmspub_utils.h"

namespace libmspub
{

Fill::Fill(const MSPUBCollector *owner) : m_owner(owner)
{
}
Fill::~Fill()
{
}
ImgFill::ImgFill(unsigned imgIndex, const MSPUBCollector *owner, bool isTexture, int rot) : Fill(owner), m_imgIndex(imgIndex), m_isTexture(isTexture), m_rotation(rot)
{
}

void ImgFill::getProperties(librevenge::RVNGPropertyList *out) const
{
  out->insert("draw:fill", "bitmap");
  if (m_imgIndex > 0 && m_imgIndex <= m_owner->m_images.size())
  {
    const std::pair<ImgType, librevenge::RVNGBinaryData> &img = m_owner->m_images[m_imgIndex - 1];
    out->insert("librevenge:mime-type", mimeByImgType(img.first));
    out->insert("draw:fill-image", img.second.getBase64Data());
    out->insert("draw:fill-image-ref-point", "top-left");
    if (! m_isTexture)
    {
      out->insert("style:repeat", "stretch");
    }
    if (m_rotation != 0)
    {
      librevenge::RVNGString sValue;
      sValue.sprintf("%d", m_rotation);
      out->insert("librevenge:rotate", sValue);
    }
  }
}

PatternFill::PatternFill(unsigned imgIndex, const MSPUBCollector *owner, ColorReference fg, ColorReference bg) : ImgFill(imgIndex, owner, true, 0), m_fg(fg), m_bg(bg)
{
}

void PatternFill::getProperties(librevenge::RVNGPropertyList *out) const
{
  Color fgColor = m_fg.getFinalColor(m_owner->m_paletteColors);
  Color bgColor = m_bg.getFinalColor(m_owner->m_paletteColors);
  out->insert("draw:fill", "bitmap");
  if (m_imgIndex > 0 && m_imgIndex <= m_owner->m_images.size())
  {
    const std::pair<ImgType, librevenge::RVNGBinaryData> &img = m_owner->m_images[m_imgIndex - 1];
    const ImgType &type = img.first;
    const librevenge::RVNGBinaryData *data = &img.second;
    // fix broken MSPUB DIB by putting in correct fg and bg colors
    librevenge::RVNGBinaryData fixedImg;
    if (type == DIB && data->size() >= 0x36 + 8)
    {
      fixedImg.append(data->getDataBuffer(), 0x36);
      fixedImg.append(fgColor.b);
      fixedImg.append(fgColor.g);
      fixedImg.append(fgColor.r);
      fixedImg.append(static_cast<unsigned char>('\0'));
      fixedImg.append(bgColor.b);
      fixedImg.append(bgColor.g);
      fixedImg.append(bgColor.r);
      fixedImg.append(static_cast<unsigned char>('\0'));
      fixedImg.append(data->getDataBuffer() + 0x36 + 8, data->size() - 0x36 - 8);
      data = &fixedImg;
    }
    out->insert("librevenge:mime-type", mimeByImgType(type));
    out->insert("draw:fill-image", data->getBase64Data());
    out->insert("draw:fill-image-ref-point", "top-left");
  }
}

Pattern88Fill::Pattern88Fill(const MSPUBCollector *owner, uint8_t const(&data)[8], ColorReference const &col0, ColorReference const &col1)
  : Fill(owner)
  , m_col0(col0)
  , m_col1(col1)
{
  for (int i=0; i<8; ++i) m_data[i]=data[i];
}

void Pattern88Fill::getProperties(librevenge::RVNGPropertyList *out) const
{
  out->insert("draw:fill", "bitmap");
  librevenge::RVNGBinaryData data=createPNGForSimplePattern(m_data, m_col0.getFinalColor(m_owner->m_paletteColors), m_col1.getFinalColor(m_owner->m_paletteColors));
  out->insert("librevenge:mime-type", mimeByImgType(PNG));
  out->insert("draw:fill-image", data.getBase64Data());
  out->insert("draw:fill-image-ref-point", "top-left");
}

SolidFill::SolidFill(ColorReference color, double opacity, const MSPUBCollector *owner) : Fill(owner), m_color(color), m_opacity(opacity)
{
}

void SolidFill::getProperties(librevenge::RVNGPropertyList *out) const
{
  Color fillColor = m_color.getFinalColor(m_owner->m_paletteColors);
  out->insert("draw:fill", "solid");
  out->insert("draw:fill-color", MSPUBCollector::getColorString(fillColor));
  librevenge::RVNGString val;
  val.sprintf("%d%%", int(m_opacity * 100));
  out->insert("draw:opacity", val);
  out->insert("svg:fill-rule", "nonzero");
}

GradientFill::GradientFill(const MSPUBCollector *owner, double angle, int type) : Fill(owner), m_stops(), m_style(GradientFill::G_None), m_angle(angle), m_radius(), m_type(type), m_fillLeftVal(0.0), m_fillTopVal(0.0), m_fillRightVal(0.0), m_fillBottomVal(0.0)
{
}

GradientFill::GradientFill(const MSPUBCollector *owner, Style style, double angle, boost::optional<double> cx, boost::optional<double> cy)
  : Fill(owner)
  , m_stops()
  , m_style(style)
  , m_angle(angle)
  , m_radius()
  , m_type(7)
  , m_fillLeftVal(0.0)
  , m_fillTopVal(0.0)
  , m_fillRightVal(0.0)
  , m_fillBottomVal(0.0)
{
  m_center[0]=cx;
  m_center[1]=cy;
}

void GradientFill::setFillCenter(double left, double top, double right, double bottom)
{
  m_fillLeftVal = left;
  m_fillTopVal = top;
  m_fillRightVal = right;
  m_fillBottomVal = bottom;
}

void GradientFill::addColor(ColorReference c, unsigned offsetPercent, double opacity)
{
  m_stops.push_back(StopInfo(c, offsetPercent, opacity));
}

void GradientFill::addColorReverse(ColorReference c, unsigned offsetPercent, double opacity)
{
  m_stops.insert(m_stops.begin(), StopInfo(c, offsetPercent, opacity));
}

void GradientFill::completeComplexFill()
{
  size_t stops = m_stops.size();
  for (auto i = stops; i > 0; i--)
  {
    if (m_stops[i-1].m_offsetPercent != 50)
      m_stops.push_back(StopInfo(m_stops[i-1].m_colorReference, 50 - m_stops[i-1].m_offsetPercent + 50, m_stops[i-1].m_opacity));
  }
}

void GradientFill::getProperties(librevenge::RVNGPropertyList *out) const
{
  librevenge::RVNGPropertyListVector ret;
  out->insert("draw:fill", "gradient");
  out->insert("svg:fill-rule", "nonzero");
  out->insert("draw:angle", -m_angle); // draw:angle is clockwise in odf format
  switch (m_style)
  {
  case G_Axial:
    out->insert("draw:style", "axial");
    break;
  case G_Radial:
    out->insert("draw:style", "radial");
    break;
  case G_Rectangular:
    out->insert("draw:style", "rectangular");
    break;
  case G_Square:
    out->insert("draw:style", "square");
    break;
  case G_Ellipsoid:
    out->insert("draw:style", "ellipsoid");
    break;
  case G_Linear:
    out->insert("draw:style", "linear");
    break;
  case G_None:
#if !defined(__clang__)
  default:
#endif
    // CHANGE: change the 2002 parser then add a warning here
    break;
  }
  for (int i=0; i<2; ++i)
  {
    if (!m_center[i]) continue;
    out->insert(i==0 ? "svg:cx" : "svg:cy", *m_center[i], librevenge::RVNG_PERCENT);
  }
  if (m_radius)
    out->insert("svg:r", *m_radius, librevenge::RVNG_PERCENT);
  switch (m_type)
  {
  // CHANGEME: nobody use that
  case SHADE_CENTER:
    out->insert("libmspub:shade", "center");
    if ((m_fillLeftVal > 0.5) && (m_fillTopVal > 0.5) && (m_fillRightVal > 0.5) && (m_fillBottomVal > 0.5))
      out->insert("libmspub:shade-ref-point", "bottom-right");
    else if ((m_fillLeftVal < 0.5) && (m_fillTopVal < 0.5) && (m_fillRightVal < 0.5) && (m_fillBottomVal < 0.5))
      out->insert("libmspub:shade-ref-point", "top-left");
    else if ((m_fillLeftVal > 0.5) && (m_fillTopVal < 0.5) && (m_fillRightVal > 0.5) && (m_fillBottomVal < 0.5))
      out->insert("libmspub:shade-ref-point", "top-right");
    else if ((m_fillLeftVal < 0.5) && (m_fillTopVal > 0.5) && (m_fillRightVal < 0.5) && (m_fillBottomVal > 0.5))
      out->insert("libmspub:shade-ref-point", "bottom-left");
    break;
  case SHADE_SHAPE:
    out->insert("libmspub:shade", "shape");
    break;
  case SHADE:
  case SHADE_SCALE:
  default:
    out->insert("libmspub:shade", "normal");
    break;
  }
  for (const auto &stop : m_stops)
  {
    Color c = stop.m_colorReference.getFinalColor(m_owner->m_paletteColors);
    librevenge::RVNGPropertyList stopProps;
    librevenge::RVNGString sValue;
    sValue.sprintf("%d%%", stop.m_offsetPercent);
    stopProps.insert("svg:offset", sValue);
    stopProps.insert("svg:stop-color", MSPUBCollector::getColorString(c));
    sValue.sprintf("%d%%", int(stop.m_opacity * 100));
    stopProps.insert("svg:stop-opacity", sValue);
    ret.append(stopProps);
  }
  out->insert("svg:linearGradient", ret);
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
