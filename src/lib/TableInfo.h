/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_TABLEINFO_H
#define INCLUDED_TABLEINFO_H

#include <vector>

#include <librevenge/librevenge.h>

#include "ColorReference.h"
#include "Line.h"

namespace libmspub
{

struct CellInfo
{
  CellInfo()
    : m_startRow()
    , m_endRow()
    , m_startColumn()
    , m_endColumn()
  {
  }

  unsigned m_startRow;
  unsigned m_endRow;
  unsigned m_startColumn;
  unsigned m_endColumn;
};

struct CellStyle
{
  CellStyle()
    : m_borders()
    , m_color()
    , m_flags(0)
  {
  }
  std::vector<Line> m_borders;
  boost::optional<ColorReference> m_color;
  unsigned m_flags;
  /** add the style to cell properties (if possible) */
  void addTo(librevenge::RVNGPropertyList &propList, std::vector<Color> const &palette) const;
};

struct TableInfo
{
  std::vector<unsigned> m_rowHeightsInEmu;
  std::vector<unsigned> m_columnWidthsInEmu;
  unsigned m_numRows;
  unsigned m_numColumns;
  std::vector<CellInfo> m_cells;
  bool m_tableCoveredCellHasTextFlag;

  TableInfo(unsigned numRows, unsigned numColumns) : m_rowHeightsInEmu(),
    m_columnWidthsInEmu(), m_numRows(numRows), m_numColumns(numColumns),
    m_cells(), m_tableCoveredCellHasTextFlag(false)
  {
  }
};
} // namespace libmspub

#endif /* INCLUDED_TABLEINFO_H */
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
