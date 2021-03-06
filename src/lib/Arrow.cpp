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
#include "VectorTransformation2D.h"

namespace libmspub
{
namespace
{
std::ostream &operator<<(std::ostream &s, Vector2D const &pt)
{
  s << int(pt.m_x+0.5) << " " << int(pt.m_y+0.5);
  return s;
}
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
  VectorTransformation2D transf=VectorTransformation2D::fromScaling(dim[0],dim[1]);
  std::stringstream s;
  if (m_style==LINE_ARROW)
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(20,20));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      0,20, 10,0, 20,20,
      18,20, 11,4, 9,4, 2,20
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,20*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << transf.transform(Vector2D(coords[i],coords[i+1]));
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), true);
  }
  else if (m_style==ROTATED_SQUARE_ARROW)
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(20,20));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      0,10, 10,0, 20,10, 10,20
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,20*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << transf.transform(Vector2D(coords[i],coords[i+1]));
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
  }
  else if (m_style==KITE_ARROW)
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(20,20));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      0,13, 10,0, 20,13, 10,20
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,20*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << transf.transform(Vector2D(coords[i],coords[i+1]));
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
  }
  else if (m_style==FAT_LINE_ARROW)
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(20,20));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      10,0, 20,10, 20,20, 10,10, 0,20, 0,10
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,20*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << transf.transform(Vector2D(coords[i],coords[i+1]));
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
  }
  else if (m_style==BLOCK_ARROW)
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(20,20));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      0,20, 20,20, 20,5, 10,0, 0,5
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,20*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << transf.transform(Vector2D(coords[i],coords[i+1]));
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
  }
  else if (m_style==TRIANGLE1_ARROW)
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(1150,1580));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      1013,1491,
      118,89, -567,-1580, -564,1580, 114,-85, 136,-68, 148,-46, 161,-17, 161,13, 153,46
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,1580*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
    {
      s << (i==0 ? "M" : "l") << transf.transform(Vector2D(coords[i],coords[i+1]));
      if (i==0) transf=VectorTransformation2D::fromScaling(dim[0],m_flipY ? -dim[1] : dim[1]);
    }
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
  }
  else if (m_style==TRIANGLE2_ARROW)
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(1150,1580));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      1013,1491,
      118,89, -567,-1580, -564,1580, 114,-85, 136,-68, 148,-46, 151,-17, 10,200, 10,-200, 151,13, 153,46
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,1580*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
    {
      s << (i==0 ? "M" : "l") << transf.transform(Vector2D(coords[i],coords[i+1]));
      if (i==0) transf=VectorTransformation2D::fromScaling(dim[0],m_flipY ? -dim[1] : dim[1]);
    }
    s << "z";
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
  }
  else
  {
    s << transf.transform(Vector2D(0,0)) << " " << transf.transform(Vector2D(20,20));
    propList.insert((header+"-viewbox").c_str(), s.str().c_str());
    int const coords[]=
    {
      0,20, 10,0, 20,20,
    };
    if (m_flipY)
      transf=VectorTransformation2D(dim[0],0,0,-dim[1],0,20*dim[1]);
    s.str("");
    for (size_t i=0; i+1<MSPUB_N_ELEMENTS(coords); i+=2)
      s << (i==0 ? "M" : "L") << transf.transform(Vector2D(coords[i],coords[i+1]));
    propList.insert((header+"-path").c_str(), s.str().c_str());
    propList.insert((header+"-center").c_str(), false);
    if (m_style!=TRIANGLE_ARROW)
    {
      MSPUB_DEBUG_MSG(("Arrow::addTo: unimplemented arrow style=%d\n",m_style));
    }
  }
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
