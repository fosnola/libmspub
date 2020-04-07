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

// FIELD
static bool convertDTFormat(std::string const &dtFormat, librevenge::RVNGPropertyListVector &propVect)
{
  propVect.clear();
  std::string text("");
  librevenge::RVNGPropertyList list;
  size_t len=dtFormat.size();
  for (size_t c=0; c < len; ++c)
  {
    if (dtFormat[c]!='%' || c+1==len)
    {
      text+=dtFormat[c];
      continue;
    }
    char ch=dtFormat[++c];
    if (ch=='%')
    {
      text += '%';
      continue;
    }
    if (!text.empty())
    {
      list.clear();
      list.insert("librevenge:value-type", "text");
      list.insert("librevenge:text", text.c_str());
      propVect.append(list);
      text.clear();
    }
    list.clear();
    switch (ch)
    {
    case 'Y':
      list.insert("number:style", "long");
      MSPUB_FALLTHROUGH;
    case 'y':
      list.insert("librevenge:value-type", "year");
      propVect.append(list);
      break;
    case 'B':
      list.insert("number:style", "long");
      MSPUB_FALLTHROUGH;
    case 'b':
    case 'h':
      list.insert("librevenge:value-type", "month");
      list.insert("number:textual", true);
      propVect.append(list);
      break;
    case 'm':
      list.insert("librevenge:value-type", "month");
      propVect.append(list);
      break;
    case 'e':
      list.insert("number:style", "long");
      MSPUB_FALLTHROUGH;
    case 'd':
      list.insert("librevenge:value-type", "day");
      propVect.append(list);
      break;
    case 'A':
      list.insert("number:style", "long");
      MSPUB_FALLTHROUGH;
    case 'a':
      list.insert("librevenge:value-type", "day-of-week");
      propVect.append(list);
      break;

    case 'H':
      list.insert("number:style", "long");
      MSPUB_FALLTHROUGH;
    case 'I':
      list.insert("librevenge:value-type", "hours");
      propVect.append(list);
      break;
    case 'M':
      list.insert("librevenge:value-type", "minutes");
      list.insert("number:style", "long");
      propVect.append(list);
      break;
    case 'S':
      list.insert("librevenge:value-type", "seconds");
      list.insert("number:style", "long");
      propVect.append(list);
      break;
    case 'p':
      list.clear();
      list.insert("librevenge:value-type", "am-pm");
      propVect.append(list);
      break;
#if !defined(__clang__)
    default:
      MSPUB_DEBUG_MSG(("convertDTFormat: find unimplement command %c(ignored)\n", ch));
#endif
    }
  }
  if (!text.empty())
  {
    list.clear();
    list.insert("librevenge:value-type", "text");
    list.insert("librevenge:text", text.c_str());
    propVect.append(list);
  }
  return propVect.count()!=0;
}

bool Field::addTo(librevenge::RVNGPropertyList &propList) const
{
  switch (m_type)
  {
  case Date:
  {
    propList.insert("librevenge:field-type", "text:date");

    librevenge::RVNGPropertyListVector pVect;
    if (m_DTFormat.empty() || !convertDTFormat(m_DTFormat, pVect))
      break;

    propList.insert("librevenge:value-type", "date");
    propList.insert("number:automatic-order", "true");
    propList.insert("librevenge:format", pVect);
    break;
  }
  case PageCount:
    propList.insert("librevenge:field-type", "text:page-count");
    propList.insert("style:num-format", "1");
    break;
  case PageNumber:
    propList.insert("librevenge:field-type", "text:page-number");
    propList.insert("style:num-format", "1");
    break;
  case Time:
  {
    propList.insert("librevenge:field-type", "text:time");

    librevenge::RVNGPropertyListVector pVect;
    if (m_DTFormat.empty() || !convertDTFormat(m_DTFormat, pVect))
      break;

    propList.insert("librevenge:value-type", "time");
    propList.insert("number:automatic-order", "true");
    propList.insert("librevenge:format", pVect);
    break;
  }
  case None:
  default:
    return false;
  }
  return true;
}

}
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
