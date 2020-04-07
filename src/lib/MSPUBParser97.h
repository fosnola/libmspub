/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_MSPUBPARSER97_H
#define INCLUDED_MSPUBPARSER97_H

#include <map>
#include <utility>
#include <vector>

#include "MSPUBParser2k.h"

namespace libmspub
{
struct ListInfo;
struct CellStyle;
class MSPUBParser97 : public MSPUBParser2k
{
  enum What { LineEnd, ShapeEnd, FieldBegin, CellEnd };

  struct SpanInfo97
  {
    unsigned m_spanEnd;
    CharacterStyle m_style;
    SpanInfo97(unsigned spanEnd, const CharacterStyle &style)
      : m_spanEnd(spanEnd), m_style(style)
    {
    }
  };

  bool parseTextListHeader(librevenge::RVNGInputStream *input, unsigned long endPos, ListHeader2k &header);
  void parseShapeFormat(librevenge::RVNGInputStream *input, unsigned seqNum,
                        ChunkHeader2k const &header) override;
  bool parseSpanStyles(librevenge::RVNGInputStream *input, unsigned index,
                       std::vector<CharacterStyle> &styles, std::map<unsigned, unsigned> &posToStyle);
  bool parseParagraphStyles(librevenge::RVNGInputStream *input, unsigned index,
                            std::vector<ParagraphStyle> &styles, std::map<unsigned, unsigned> &posToStyle);
  bool parseCellStyles(librevenge::RVNGInputStream *input, unsigned index,
                       std::vector<CellStyle> &styles, std::map<unsigned, unsigned> &posToStyle);
  CharacterStyle readCharacterStyle(librevenge::RVNGInputStream *input,
                                    unsigned length);
  void parseBulletDefinitions(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input) override;
  void parseTextInfos(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input) override;
  void parseTableInfoData(librevenge::RVNGInputStream *input, unsigned seqNum, ChunkHeader2k const &header,
                          unsigned textId, unsigned numCols, unsigned numRows, unsigned width, unsigned height);
  void parseClipPath(librevenge::RVNGInputStream *input, unsigned seqNum, ChunkHeader2k const &header);
  void parseContentsTextIfNecessary(librevenge::RVNGInputStream *input) override;
  void getTextInfo(librevenge::RVNGInputStream *input, unsigned length, std::map<unsigned,MSPUBParser97::What> &posToType);
public:
  MSPUBParser97(librevenge::RVNGInputStream *input, MSPUBCollector *collector);
  bool parse() override;

protected:
  std::vector<ListInfo> m_bulletLists;
  std::map<unsigned, unsigned> m_chunkIdToTextEndMap;
};
}

#endif //  INCLUDED_MSPUBPARSER97_H

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
