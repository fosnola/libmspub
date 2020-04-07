/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_MSPUBPARSER2K_H
#define INCLUDED_MSPUBPARSER2K_H

#include <deque>
#include <vector>
#include <map>

#include "MSPUBParser.h"
#include "ShapeType.h"

namespace libmspub
{
class ColorReference;

class MSPUBParser2k : public MSPUBParser
{
public:
  enum ChunkType2k
  {
    C_Text /* 0 or 8*/, C_Table, C_Image, C_OLE, C_Line, C_Rect, C_CustomShape, C_Ellipse,
    C_Group, C_Document, C_Page, C_Unknown
  };
  struct ChunkHeader2k
  {
    ChunkHeader2k()
      : m_type(C_Unknown)
      , m_fileType(0xffff)
      , m_beginOffset(0)
      , m_maxHeaderSize(0)
      , m_dataOffset(0)
      , m_endOffset(0)
      , m_flagOffset(0)
    {
    }
    bool isRectangle() const
    {
      return m_type==C_Text || m_type==C_Table || m_type==C_Image || m_type==C_OLE || m_type==C_Rect;
    }
    bool isShape() const
    {
      return isRectangle() || m_type==C_Line || m_type==C_CustomShape;
    }
    unsigned headerLength() const
    {
      return m_beginOffset<m_dataOffset ? m_dataOffset-m_beginOffset : 0;
    }
    bool hasData() const
    {
      return m_dataOffset!=0 && m_dataOffset<m_endOffset;
    }
    ChunkType2k m_type;
    unsigned m_fileType;
    unsigned m_beginOffset;
    unsigned m_maxHeaderSize;
    unsigned m_dataOffset;
    unsigned m_endOffset;
    unsigned m_flagOffset; // removeme
  };
  struct ListHeader2k
  {
    ListHeader2k()
      : m_dataOffset(0)
      , m_pointerSize(2)
      , m_N(0)
      , m_maxN(0)
      , m_dataSize(0)
      , m_positions()
    {
      for (auto &v : m_values) v=0;
    }
    //! the data begin offset
    unsigned m_dataOffset;
    //! the pointer size
    unsigned m_pointerSize;
    //! the number of data
    unsigned m_N;
    //! the maximum data
    unsigned m_maxN;
    //! the data size (or the last position)
    unsigned m_dataSize;
    //! two unknown value
    int m_values[2];
    //! the position
    std::vector<unsigned> m_positions;
  };

private:
  std::vector<unsigned> m_imageDataChunkIndices;
  std::vector<unsigned> m_oleDataChunkIndices;
  boost::optional<unsigned> m_specialPaperChunkIndex;
  std::vector<unsigned> m_quillColorEntries;
  std::map<unsigned, unsigned> m_fileIdToChunkId;
  std::map<unsigned, std::vector<unsigned> > m_chunkChildIndicesById;
  std::set<unsigned> m_shapesAlreadySend;

protected:
  unsigned m_version; // v2:2, v3:3, 97:4, 98:5, 2000:6
  bool m_isBanner;
  std::map<unsigned, unsigned> m_chunkIdToTextEndMap;

  // helper functions
  static ShapeType getShapeType(unsigned char shapeSpecifier);
  bool getChunkReference(unsigned seqNum, ContentChunkReference &chunk) const;
  bool parse2kShapeChunk(ContentChunkReference const &chunk, librevenge::RVNGInputStream *input,
                         boost::optional<unsigned> pageSeqNum = boost::optional<unsigned>(),
                         bool topLevelCall = true);
  void parseChunkHeader(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input,
                        ChunkHeader2k &header);
  virtual void updateVersion(int docChunkSize, int contentVersion);
  virtual void parseBulletDefinitions(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input);
  virtual void parseTextInfos(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input);
  void parseShapeFormat(librevenge::RVNGInputStream *input, unsigned seqNum,
                        ChunkHeader2k const &header);
  virtual void parseTableInfoData(librevenge::RVNGInputStream *input, unsigned seqNum, ChunkHeader2k const &header,
                                  unsigned textId, unsigned numCols, unsigned numRows, unsigned width, unsigned height);
  virtual void parseClipPath(librevenge::RVNGInputStream *input, unsigned seqNum, ChunkHeader2k const &header);

  bool parseGroup(librevenge::RVNGInputStream *input, unsigned seqNum, unsigned page);
  bool parseBorderArts(librevenge::RVNGInputStream *input);
  bool parseContents(librevenge::RVNGInputStream *input) override;
  bool parseDocument(librevenge::RVNGInputStream *input);
  bool parseFonts(librevenge::RVNGInputStream *input);
  bool parsePage(librevenge::RVNGInputStream *input, unsigned seqNum);
  unsigned getColorIndexByQuillEntry(unsigned entry) override;
  int translateCoordinateIfNecessary(int coordinate) const;
  static Color getColorBy2kIndex(unsigned char index);
  static Color getColorBy2kHex(unsigned hex);
  unsigned translate2kColorReference(unsigned ref2k) const;
  ColorReference getColorReferenceByIndex(unsigned ref2k) const;
  static PageType getPageTypeBySeqNum(unsigned seqNum);
  virtual void parseContentsTextIfNecessary(librevenge::RVNGInputStream *input);
  bool parseListHeader(librevenge::RVNGInputStream *input, unsigned long endPos, ListHeader2k &header, bool readPosition);
  bool parseIdList(librevenge::RVNGInputStream *input, unsigned long endPos, std::vector<unsigned> &ids);
  bool parseBorderArt(librevenge::RVNGInputStream *input, unsigned borderNum, unsigned endPos);

  // old code remove me
  void parseShapeLine(librevenge::RVNGInputStream *input, bool isRectangle, unsigned offset, unsigned seqNum);
  void parseShapeFill(librevenge::RVNGInputStream *input, unsigned seqNum, unsigned chunkOffset);
  void parseShapeFlips(librevenge::RVNGInputStream *input, unsigned flagsOffset, unsigned seqNum,
                       unsigned chunkOffset);
  unsigned getFirstLineOffset() const;
  unsigned getSecondLineOffset() const;
  unsigned getShapeFillTypeOffset() const;
  unsigned getShapeFillColorOffset() const;
  unsigned getTextIdOffset() const;
public:
  explicit MSPUBParser2k(librevenge::RVNGInputStream *input, MSPUBCollector *collector);
  bool parse() override;
  ~MSPUBParser2k() override;
};

} // namespace libmspub

#endif //  INCLUDED_MSPUBPARSER2K_H

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
