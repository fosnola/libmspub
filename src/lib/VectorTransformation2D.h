/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_VECTORTRANSFORMATION2D_H
#define INCLUDED_VECTORTRANSFORMATION2D_H

namespace libmspub
{
struct Vector2D
{
  double m_x;
  double m_y;
  Vector2D(double x, double y) : m_x(x), m_y(y)
  {
  }
};
Vector2D operator+(const Vector2D &l, const Vector2D &r);
Vector2D operator-(const Vector2D &l, const Vector2D &r);
class VectorTransformation2D
{
  double m_m11, m_m12, m_m21, m_m22;
  double m_x, m_y;
public:
  VectorTransformation2D();
  VectorTransformation2D(double m11, double m12, double m21, double m22, double x, double y);
  Vector2D transform(Vector2D original) const;
  Vector2D transformWithOrigin(Vector2D v, Vector2D origin) const;
  double getRotation() const;
  double getHorizontalScaling() const;
  double getVerticalScaling() const;
  bool orientationReversing() const;
  //! return true is the transformation is composed of a scaling (with no axis inversion) and a translation
  bool isSimple() const;
  friend VectorTransformation2D operator*(const VectorTransformation2D &l, const VectorTransformation2D &r);
  static VectorTransformation2D fromFlips(bool flipH, bool flipV);
  static VectorTransformation2D fromTranslate(double x, double y);
  static VectorTransformation2D fromCounterRadians(double theta);
  static VectorTransformation2D fromScaling(double x, double y);
};
VectorTransformation2D operator*(const VectorTransformation2D &l, const VectorTransformation2D &r);
} // namespace libmspub

#endif /* INCLUDED_VECTORTRANSFORMATION2D_H */
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
