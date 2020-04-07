/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sstream>

#include "MSPUBCollector.h"

#include "TableInfo.h"

namespace libmspub
{
void CellStyle::addTo(librevenge::RVNGPropertyList &propList, std::vector<Color> const &palette) const
{
  if (m_color)
    propList.insert("fo:background-color",
                    MSPUBCollector::getColorString(m_color->getFinalColor(palette)));
  for (size_t i=0; i<m_borders.size() && i<4; ++i)
  {
    auto const &line=m_borders[i];
    if (!line.m_lineExists) continue;
    std::stringstream s;
    s << double(line.m_widthInEmu)/EMUS_IN_INCH*72 << "pt";
    s << " solid";
    s << " " << MSPUBCollector::getColorString(line.m_color.getFinalColor(palette)).cstr();
    char const *(wh[])= {"left","top","right","bottom"};
    std::string prefix("fo:border-");
    propList.insert((prefix+wh[i]).c_str(), s.str().c_str());
  }
}

}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
