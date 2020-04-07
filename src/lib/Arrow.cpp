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

void Arrow::addTo(librevenge::RVNGPropertyList &propList, bool start) const
{
  if (m_style==NO_ARROW) return;
  std::string header(start ? "draw:marker-start" : "draw:marker-end");
  int dim[2];
  for (int i=0; i<2; ++i)
  {
    auto const which = i==0 ? m_height : m_width;
    dim[i]=which==SMALL ? 5 : which==MEDIUM ? 10 : 20;
  }
  propList.insert((header+"-width").c_str(), dim[1], librevenge::RVNG_POINT);
  double const arrowDim[4]= {0,0,20,30};
  bool const isCentered=false;
  std::stringstream s;
  s << int(dim[0]*arrowDim[0]) << " "
    << int(dim[1]*arrowDim[1]) << " "
    << int(dim[0]*arrowDim[2]) << " "
    << int(dim[1]*arrowDim[3]);
  propList.insert((header+"-viewbox").c_str(), s.str().c_str());
  s.str("");
  s << "m" << dim[0]*10 << " " << "0-" << dim[0]*10 << " " << dim[1]*30 << "h" << dim[0]*20 << "z";
  propList.insert((header+"-path").c_str(), s.str().c_str());
  propList.insert((header+"-center").c_str(), isCentered);
  if (m_style!=TRIANGLE_ARROW)
  {
    MSPUB_DEBUG_MSG(("Arrow::addTo: only triangle arrow is implemented\n"));
  }
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
