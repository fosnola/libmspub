/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_FILL_H
#define INCLUDED_FILL_H

#include <vector>

#include <librevenge/librevenge.h>

#include "ColorReference.h"

namespace libmspub
{
class MSPUBCollector;
class Fill
{
protected:
  const MSPUBCollector *m_owner;
public:
  Fill(const MSPUBCollector *owner);
  virtual void getProperties(librevenge::RVNGPropertyList *out) const = 0;
  virtual ~Fill();
private:
  Fill(const Fill &) : m_owner(nullptr) { }
  Fill &operator=(const Fill &);
};

class ImgFill : public Fill
{
protected:
  unsigned m_imgIndex;
private:
  bool m_isTexture;
protected:
  int m_rotation;
public:
  ImgFill(unsigned imgIndex, const MSPUBCollector *owner, bool isTexture, int rotation);
  void getProperties(librevenge::RVNGPropertyList *out) const override;
private:
  ImgFill(const ImgFill &) : Fill(nullptr), m_imgIndex(0), m_isTexture(false), m_rotation(0) { }
  ImgFill &operator=(const ImgFill &);
};

class PatternFill : public ImgFill
{
  ColorReference m_fg;
  ColorReference m_bg;
public:
  PatternFill(unsigned imgIndex, const MSPUBCollector *owner, ColorReference fg, ColorReference bg);
  void getProperties(librevenge::RVNGPropertyList *out) const override;
private:
  PatternFill(const PatternFill &) : ImgFill(0, nullptr, true, 0), m_fg(0x08000000), m_bg(0x08000000) { }
  PatternFill &operator=(const ImgFill &);
};

class Pattern88Fill : public Fill
{
  ColorReference m_col0;
  ColorReference m_col1;
  uint8_t m_data[8];
public:
  Pattern88Fill(const MSPUBCollector *owner, uint8_t const(&data)[8], ColorReference const &col0, ColorReference const &col1);
  void getProperties(librevenge::RVNGPropertyList *out) const override;
private:
  Pattern88Fill(const Pattern88Fill &) = delete;
  Pattern88Fill &operator=(const Fill &) = delete;
};

class SolidFill : public Fill
{
  ColorReference m_color;
  double m_opacity;
public:
  SolidFill(ColorReference color, double opacity, const MSPUBCollector *owner);
  void getProperties(librevenge::RVNGPropertyList *out) const override;
private:
  SolidFill(const SolidFill &) : Fill(nullptr), m_color(0x08000000), m_opacity(1) { }
  SolidFill &operator=(const SolidFill &);
};

class GradientFill : public Fill
{
public:
  enum Style { G_Axial, G_Ellipsoid, G_Linear, G_Radial, G_Rectangular, G_Square, G_None };
private:
  struct StopInfo
  {
    ColorReference m_colorReference;
    unsigned m_offsetPercent;
    double m_opacity;
    StopInfo(ColorReference colorReference, unsigned offsetPercent, double opacity) : m_colorReference(colorReference), m_offsetPercent(offsetPercent), m_opacity(opacity) { }
  };
  std::vector<StopInfo> m_stops;
  Style m_style;
  double m_angle;
  boost::optional<double> m_center[2];
  boost::optional<double> m_radius;
  // shadow
  int m_type;
  double m_fillLeftVal;
  double m_fillTopVal;
  double m_fillRightVal;
  double m_fillBottomVal;
public:
  GradientFill(const MSPUBCollector *owner, double angle = 0, int type = 7);
  GradientFill(const MSPUBCollector *owner, Style style, double angle, boost::optional<double> cx=0, boost::optional<double> cy=0);
  void setFillCenter(double left, double top, double right, double bottom);
  void addColor(ColorReference c, unsigned offsetPercent, double opacity);
  void addColorReverse(ColorReference c, unsigned offsetPercent, double opacity);
  void completeComplexFill();
  void getProperties(librevenge::RVNGPropertyList *out) const override;
private:
  GradientFill(const GradientFill &) : Fill(nullptr), m_stops(), m_style(G_None), m_angle(0), m_radius(), m_type(7), m_fillLeftVal(0.0), m_fillTopVal(0.0), m_fillRightVal(0.0), m_fillBottomVal(0.0) { }
  GradientFill &operator=(const GradientFill &);
};
}

#endif /* INCLUDED_FILL_H */
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
