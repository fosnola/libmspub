/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <boost/optional.hpp>

#include <vector>

#include "libmspub_utils.h"

#include "ListInfo.h"

namespace libmspub
{
void ListInfo::addTo(librevenge::RVNGPropertyList &level) const
{
  level.insert("fo:font-size", 1, librevenge::RVNG_PERCENT);
  if (m_listType==ORDERED)
  {
    auto numberingType=m_numberingType.get_value_or(STANDARD_WESTERN);
    switch (numberingType)
    {
    case STANDARD_WESTERN:
      level.insert("style:num-format", "1");
      break;
    case UPPERCASE_ROMAN:
      level.insert("style:num-format", "I");
      break;
    case LOWERCASE_ROMAN:
      level.insert("style:num-format", "i");
      break;
    case UPPERCASE_LETTERS:
      level.insert("style:num-format", "A");
      break;
    case LOWERCASE_LETTERS:
      level.insert("style:num-format", "a");
      break;
    // TODO
    case STANDARD_WESTERN_AT_LEAST_TWO_DIGITS:
    case ORDINALS:
    case SPELLED_CARDINALS:
    case SPELLED_ORDINALS:
    default:
      level.insert("style:num-format", "1");
      break;
    }
    auto numberingDelimiter=m_numberingDelimiter.get_value_or(NO_DELIMITER);
    switch (numberingDelimiter)
    {
    case NO_DELIMITER:
    default:
      break;
    case PARENTHESIS:
      level.insert("style:num-suffix",")");
      break;
    case PARENTHESES_SURROUND:
      level.insert("style:num-prefix","(");
      level.insert("style:num-suffix",")");
      break;
    case PERIOD:
      level.insert("style:num-suffix",".");
      break;
    case SQUARE_BRACKET:
      level.insert("style:num-suffix","]");
      break;
    case SQUARE_BRACKET_SURROUND:
      level.insert("style:num-prefix","[");
      level.insert("style:num-suffix","]");
      break;
    case COLON:
      level.insert("style:num-suffix",":");
      break;
    case HYPHEN_SURROUND:
      level.insert("style:num-prefix","-");
      level.insert("style:num-suffix","-");
      break;
    case IDEOGRAPHIC_HALF_COMMA:   // CHECKME
    {
      librevenge::RVNGString suffix;
      appendUCS4(suffix, 0xff64);
      level.insert("style:num-suffix",suffix);
      break;
    }
    }
    if (m_numberIfRestarted)
      level.insert("text:start-value", int(*m_numberIfRestarted)+1);
  }
  else
  {
    unsigned bulletChar=m_bulletChar.get_value_or(0x2022);
    librevenge::RVNGString bullet;
    appendUCS4(bullet, bulletChar);
    level.insert("text:bullet-char", bullet);
  }
}

bool ListInfo::isCompatibleWith(ListInfo const &listInfo) const
{
  if (m_listType!=listInfo.m_listType) return false;
  if (m_listType==ORDERED)
  {
    if (bool(m_numberingType)!=bool(listInfo.m_numberingType)) return false;
    if (m_numberingType && *m_numberingType!=*listInfo.m_numberingType) return false;
    if (bool(m_numberingDelimiter)!=bool(listInfo.m_numberingDelimiter)) return false;
    if (m_numberingDelimiter && *m_numberingDelimiter!=*listInfo.m_numberingDelimiter) return false;
    if (m_numberIfRestarted && (!listInfo.m_numberIfRestarted || *listInfo.m_numberIfRestarted != *m_numberIfRestarted)) return false;
    return true;
  }
  if (bool(m_bulletChar)!=bool(listInfo.m_bulletChar)) return false;
  if (m_bulletChar && *m_bulletChar!=*listInfo.m_bulletChar) return false;
  return true;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
