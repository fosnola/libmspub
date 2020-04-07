/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sstream>

#include "Arrow.h"

#include "MSPUBConstants.h"
#include "libmspub_utils.h"

namespace libmspub
{
namespace
{
struct PointArrow
{
  PointArrow(double x, double y, int const(&factor)[2])
    : m_x(x*factor[0])
    , m_y(y*factor[1])
  {
  }
  friend std::ostream &operator<<(std::ostream &s, PointArrow const &pt)
  {
    s << int(pt.m_x+0.5) << " " << int(pt.m_y+0.5);
    return s;
  }
  double m_x;
  double m_y;
};
}
void Arrow::addTo(librevenge::RVNGPropertyList &propList, bool start) const
{
  if (m_style==NO_ARROW) return;
  std::string header(start ? "draw:marker-start" : "draw:marker-end");
  int dim[2];
  for (int i=0; i<2; ++i)
  {
    auto const which = i==1 ? m_height : m_width;
    dim[i]=which==SMALL ? 5 : which==MEDIUM ? 10 : 20;
  }
  propList.insert((header+"-width").c_str(), dim[0], librevenge::RVNG_POINT);
  std::stringstream s;
  if (m_style==LINE_ARROW)
  {
    s << PointArrow(0,0,dim) << " " << PointArrow(20,20,dim);
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      0,20, 10,0, 20,20,
      18,20, 11,4, 11,20,
      9,20, 9,4, 2,20
    };
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << PointArrow(coords[i],coords[i+1],dim);
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
  }
  else
  {
    s << PointArrow(0,0,dim) << " " << PointArrow(20,20,dim);
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      0,20, 10,0, 20,20,
    };
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << PointArrow(coords[i],coords[i+1],dim);
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
    if (m_style!=TRIANGLE_ARROW)
    {
      MSPUB_DEBUG_MSG(("Arrow::addTo: only triangle arrow is implemented\n"));
    }
  }
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
