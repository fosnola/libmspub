/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "libmspub_utils.h"

#include "MSPUBTypes.h"

namespace libmspub
{
bool EmbeddedObject::addTo(librevenge::RVNGPropertyList &propList) const
{
  bool firstSet=false;
  librevenge::RVNGPropertyListVector auxiliarVector;
  for (size_t i=0; i<m_dataList.size(); ++i)
  {
    if (m_dataList[i].empty()) continue;
    std::string type=m_typeList.size() ? m_typeList[i] : "image/pict";
    if (!firstSet)
    {
      propList.insert("librevenge:mime-type", type.c_str());
      propList.insert("office:binary-data", m_dataList[i]);
      firstSet=true;
      continue;
    }
    librevenge::RVNGPropertyList auxiList;
    auxiList.insert("librevenge:mime-type", type.c_str());
    auxiList.insert("office:binary-data", m_dataList[i]);
    auxiliarVector.append(auxiList);
  }
  if (!auxiliarVector.empty())
    propList.insert("librevenge:replacement-objects", auxiliarVector);
  if (!firstSet)
  {
    MSPUB_DEBUG_MSG(("EmbeddedObject::addTo: called without picture\n"));
    return false;
  }
  return true;
}
}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
