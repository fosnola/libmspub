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
struct ListHeader2k;

class MSPUBParser2k : public MSPUBParser
{
public:
  enum ChunkType2k
  {
    C_Text /* 0 or 8*/, C_Image, C_OLE, C_Line, C_Rect, C_CustomShape, C_Ellipse,
    C_Group, C_Document, C_Page, C_Unknown
  };
  struct ChunkHeader2k
  {
    ChunkHeader2k()
      : m_type(C_Unknown)
      , m_fileType(0xffff)
      , m_beginOffset(0)
      , m_dataOffset(0)
      , m_endOffset(0)
      , m_flagOffset(0)
    {
    }
    bool isRectangle() const
    {
      return m_type==C_Text || m_type==C_Image || m_type==C_OLE || m_type==C_Rect;
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
    unsigned m_dataOffset;
    unsigned m_endOffset;
    unsigned m_flagOffset; // removeme
  };
private:
  std::vector<unsigned> m_imageDataChunkIndices;
  std::vector<unsigned> m_oleDataChunkIndices;
  std::vector<unsigned> m_quillColorEntries;
  std::map<unsigned, std::vector<unsigned> > m_chunkChildIndicesById;
  std::deque<unsigned> m_chunksBeingRead;

protected:
  unsigned m_version;
  bool m_isBanner;

  // helper functions
  static ShapeType getShapeType(unsigned char shapeSpecifier);
  bool parse2kShapeChunk(ContentChunkReference const &chunk, librevenge::RVNGInputStream *input,
                         boost::optional<unsigned> pageSeqNum = boost::optional<unsigned>(),
                         bool topLevelCall = true);
  void parseShapeLine(librevenge::RVNGInputStream *input, bool isRectangle, unsigned offset, unsigned seqNum);
  void parseChunkHeader(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input,
                        ChunkHeader2k &header);
  virtual void parseShapeFormat(librevenge::RVNGInputStream *input, unsigned seqNum,
                                ChunkHeader2k const &header);
  void parseShapeFlips(librevenge::RVNGInputStream *input, unsigned flagsOffset, unsigned seqNum,
                       unsigned chunkOffset);
  bool parseGroup(librevenge::RVNGInputStream *input, unsigned seqNum, unsigned page);
  void parseShapeFill(librevenge::RVNGInputStream *input, unsigned seqNum, unsigned chunkOffset);
  bool parseBorderArts(librevenge::RVNGInputStream *input);
  bool parseContents(librevenge::RVNGInputStream *input) override;
  bool parseDocument(librevenge::RVNGInputStream *input);
  unsigned getColorIndexByQuillEntry(unsigned entry) override;
  int translateCoordinateIfNecessary(int coordinate) const;
  virtual unsigned getFirstLineOffset() const;
  virtual unsigned getSecondLineOffset() const;
  virtual unsigned getShapeFillTypeOffset() const;
  virtual unsigned getShapeFillColorOffset() const;
  virtual unsigned getTextIdOffset() const;
  static ColorReference getColorReferenceBy2kIndex(unsigned char index);
  static Color getColorBy2kIndex(unsigned char index);
  static Color getColorBy2kHex(unsigned hex);
  static unsigned translate2kColorReference(unsigned ref2k);
  static PageType getPageTypeBySeqNum(unsigned seqNum);
  virtual void parseContentsTextIfNecessary(librevenge::RVNGInputStream *input);
  bool parseListHeader(librevenge::RVNGInputStream *input, unsigned long endPos, ListHeader2k &header, bool readPosition);
  bool parseBorderArt(librevenge::RVNGInputStream *input, unsigned borderNum, unsigned endPos);
public:
  explicit MSPUBParser2k(librevenge::RVNGInputStream *input, MSPUBCollector *collector);
  bool parse() override;
  ~MSPUBParser2k() override;
};

} // namespace libmspub

#endif //  INCLUDED_MSPUBPARSER2K_H

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
