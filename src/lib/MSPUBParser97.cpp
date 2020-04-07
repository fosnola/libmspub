/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "MSPUBParser97.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>

#include "MSPUBCollector.h"
#include "MSPUBTypes.h"
#include "TableInfo.h"
#include "libmspub_utils.h"

namespace libmspub
{

MSPUBParser97::MSPUBParser97(librevenge::RVNGInputStream *input, MSPUBCollector *collector)
  : MSPUBParser2k(input, collector)
{
  m_collector->useEncodingHeuristic();
}

unsigned MSPUBParser97::getTextIdOffset() const
{
  return 0x46;
}

bool MSPUBParser97::parse()
{
  std::unique_ptr<librevenge::RVNGInputStream> contents(m_input->getSubStreamByName("Contents"));
  if (!contents)
  {
    MSPUB_DEBUG_MSG(("Couldn't get contents stream.\n"));
    return false;
  }
  if (!parseContents(contents.get()))
  {
    MSPUB_DEBUG_MSG(("Couldn't parse contents stream.\n"));
    return false;
  }
  return m_collector->go();
}

void MSPUBParser97::parseContentsTextIfNecessary(librevenge::RVNGInputStream *input)
{
  input->seek(0x12, librevenge::RVNG_SEEK_SET);
  unsigned blockStart=readU32(input);
  input->seek(blockStart+4, librevenge::RVNG_SEEK_SET);
  unsigned version=readU16(input);
  if (version>=200 && version<300) m_version=2; // mspub 2
  // fixme: find also a method to differentiate mspub 3 and 97
  if (m_version<=2)
  {
    // set the default parameter
    CharacterStyle defaultCharStyle;
    defaultCharStyle.textSizeInPt=10;
    m_collector->addDefaultCharacterStyle(defaultCharStyle);
  }
  input->seek(blockStart+14, librevenge::RVNG_SEEK_SET);
  unsigned textStart = readU32(input);
  unsigned textEnd = readU32(input);
  unsigned index[4];
  for (auto &i : index) i=readU16(input);

  std::vector<CharacterStyle> spanStyles;
  std::map<unsigned, unsigned> posToSpanMap;
  for (unsigned id=index[0]; id<index[1]; ++id)
    parseSpanStyles(input, id, spanStyles, posToSpanMap);
  std::vector<ParagraphStyle> paraStyles;
  std::map<unsigned, unsigned> posToParaMap;
  if (m_version<=2)
  {
    for (unsigned id=index[1]; id<index[2]; ++id)
      parseParagraphStyles(input, id, paraStyles, posToParaMap);
    //for (unsigned id=index[2]; id<index[3]; ++id)
    //  parseCellStyles(input, id, paraStyles, posToParaMap);
  }

  input->seek(textStart, librevenge::RVNG_SEEK_SET);
  std::map<unsigned,MSPUBParser97::What> posToTypeMap;
  getTextInfo(input, textEnd - textStart, posToTypeMap);

  input->seek(textStart, librevenge::RVNG_SEEK_SET);
  unsigned length = std::min(textEnd - textStart, m_length); // sanity check
  unsigned shape=0;
  std::vector<TextParagraph> shapeParas;
  std::vector<TextSpan> paraSpans;
  std::vector<unsigned char> spanChars;
  std::vector<unsigned> cellEnds;
  CharacterStyle charStyle;
  ParagraphStyle paraStyle;

  unsigned oldParaPos=0; // used to check for empty line
  unsigned actChar=0;
  for (unsigned c=0; c<length; ++c)
  {
    // change of style
    auto actPos=input->tell();
    auto cIt=posToSpanMap.find(actPos);
    if (cIt!=posToSpanMap.end())
    {
      if (!spanChars.empty())
      {
        actChar+=spanChars.size();
        paraSpans.push_back(TextSpan(spanChars,charStyle));
        spanChars.clear();
      }
      if (cIt->second<spanStyles.size())
        charStyle=spanStyles[cIt->second];
    }
    auto pIt=posToParaMap.find(actPos);
    auto specialIt=posToTypeMap.find(c);
    if (pIt!=posToParaMap.end())
    {
      if (!spanChars.empty())
      {
        actChar+=spanChars.size();
        paraSpans.push_back(TextSpan(spanChars,charStyle));
        spanChars.clear();
      }
      bool needNewPara=!paraSpans.empty();
      if (!needNewPara && oldParaPos>=actPos-3)   // potential empty line
      {
        auto sIt=specialIt;
        if (sIt!=posToTypeMap.end() && sIt->second!=ShapeEnd) ++sIt;
        needNewPara=sIt!=posToTypeMap.end() && sIt->second!=ShapeEnd;
      }
      if (needNewPara)
      {
        shapeParas.push_back(TextParagraph(paraSpans, paraStyle));
        paraSpans.clear();
      }
      if (pIt!=posToParaMap.end() && pIt->second<paraStyles.size()) paraStyle=paraStyles[pIt->second];
      oldParaPos=actPos;
    }
    unsigned char ch=readU8(input);
    // special character
    if (specialIt!=posToTypeMap.end())
    {
      if (!spanChars.empty())
      {
        actChar+=spanChars.size();
        paraSpans.push_back(TextSpan(spanChars,charStyle));
        spanChars.clear();
      }
      if (specialIt->second==FieldBegin)   // # 05
      {
        TextSpan pageSpan(spanChars,charStyle);
        pageSpan.isPageField=true;
        paraSpans.push_back(pageSpan);
        input->seek(1, librevenge::RVNG_SEEK_CUR);
        ++c;
        continue;
      }
      if (!paraSpans.empty())   // end of line or shape
      {
        shapeParas.push_back(TextParagraph(paraSpans, paraStyle));
        paraSpans.clear();
      }
      if (specialIt->second==CellEnd)
        cellEnds.push_back(actChar+1); // offset begin at 1...
      if (specialIt->second==ShapeEnd)
      {
        if (!cellEnds.empty())
        {
          m_collector->setTableCellTextEnds(shape,cellEnds);
          cellEnds.clear();
        }
        m_collector->addTextString(shapeParas, shape++);
        charStyle=CharacterStyle();
        shapeParas.clear();
        actChar=0;
      }
      continue;
    }
    if (ch == 0xB) // Pub97 interprets vertical tab as nonbreaking space.
      spanChars.push_back('\n');
    else if (ch == 0x0C)
    {
      // end of shape
    }
    else if (ch == 0x0D || ch==0x0A)
    {
      // 0d 0a end of line (but also end of paragraph)
    }
    else if (ch==0x9 || ch==0xf || ch>0x1f) // 0xf: means cells separator
    {
      spanChars.push_back(ch);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseContentsTextIfNecessary:find odd character %x\n", unsigned(ch)));
    }
  }
  // check if no data remain
  if (!spanChars.empty())
  {
    actChar+=spanChars.size();
    paraSpans.push_back(TextSpan(spanChars,charStyle));
  }
  if (!paraSpans.empty())
    shapeParas.push_back(TextParagraph(paraSpans, paraStyle));
  if (!shapeParas.empty())
  {
    if (!cellEnds.empty())
      m_collector->setTableCellTextEnds(shape,cellEnds);
    m_collector->addTextString(shapeParas, shape++);
  }
}

bool MSPUBParser97::parseParagraphStyles(librevenge::RVNGInputStream *input, unsigned index,
                                         std::vector<ParagraphStyle> &styles,
                                         std::map<unsigned, unsigned> &posToStyle)
{
  if (input->seek((index+1)*0x200-1, librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: can not use index=%x\n", index));
    return false;
  }
  unsigned N=unsigned(readU8(input));
  if ((N+1)*5>0x200)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: N value=%d is too big for index=%x\n", N, index));
    return false;
  }
  input->seek(index*0x200, librevenge::RVNG_SEEK_SET);
  std::vector<unsigned> positions;
  positions.resize(N+1);
  for (auto &p : positions) p=readU32(input);
  std::vector<uint8_t> stylePos;
  stylePos.reserve(N);
  for (unsigned i=0; i<N; ++i) stylePos.push_back(readU8(input));
  if (styles.empty()) // create an empty style
    styles.push_back(ParagraphStyle());
  std::map<unsigned, unsigned> offsetToStyleMap;
  offsetToStyleMap[0]=0;
  for (unsigned i=0; i<N; ++i)
  {
    auto const &offs=stylePos[i];
    auto oIt=offsetToStyleMap.find(offs);
    if (oIt!=offsetToStyleMap.end())
    {
      posToStyle[positions[i]]=oIt->second;
      continue;
    }
    input->seek(index*0x200+2*offs, librevenge::RVNG_SEEK_SET); // skip small size
    uint8_t len=readU8(input);
    uint8_t tabPos=readU8(input);
    if (tabPos<2 || 2*offs+1+tabPos>0x200 || 2*len+1<tabPos)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: can not read len for i=%x for index=%x\n", i, index));
      posToStyle[positions[i]]=0;
      continue;
    }
    input->seek(1,librevenge::RVNG_SEEK_CUR); // 0
    ParagraphStyle style;
    if (tabPos>=3)
    {
      int align=int(readU8(input));
      switch (align)
      {
      case 0:
        style.m_align=LEFT;
        break;
      case 1:
        style.m_align=CENTER;
        break;
      case 2:
        style.m_align=RIGHT;
        break;
      case 3:
        style.m_align=JUSTIFY;
        break;
      default:
        MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: unknown align=%d\n", align));
        break;
      }
      // &20 tight/loose/...
    }
    if (tabPos>=5)
      style.m_rightIndentEmu=readU16(input)*635;
    if (tabPos>=7)
      style.m_leftIndentEmu=readU16(input)*635;
    if (tabPos>=9)
      style.m_firstLineIndentEmu=readS16(input)*635;
    if (tabPos>=11)
    {
      unsigned spacing=readU16(input);
      if (spacing&0x8000)
        style.m_lineSpacing=LineSpacingInfo(LINE_SPACING_PT,double(0x10000-spacing)/20.);
      else if (spacing)
        style.m_lineSpacing=LineSpacingInfo(LINE_SPACING_SP,double(spacing)/240.);
    }
    if (tabPos>=13)
    {
      style.m_spaceBeforeEmu=readU8(input)*635;
      input->seek(1, librevenge::RVNG_SEEK_CUR);
    }
    if (tabPos>=15)
    {
      style.m_spaceAfterEmu=readU8(input)*635;
      input->seek(1, librevenge::RVNG_SEEK_CUR);
    }
    if (1+tabPos+3<2*len+1)
    {
      input->seek(index*0x200+2*offs+1+tabPos, librevenge::RVNG_SEEK_SET);
      auto tabLen=readU8(input);
      if (tabLen<2 || 2*offs+1+tabPos+1+tabLen>0x200 || 2*len+1<tabPos+1+tabLen)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: can not read tab len for i=%x for index=%x\n", i, index));
      }
      else
      {
        input->seek(1, librevenge::RVNG_SEEK_CUR);
        auto nTabs=readU8(input);
        if (3*nTabs+2>tabLen)
        {
          MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: bad tabs numbers=%d for index=%x\n", int(nTabs), index));
        }
        else
        {
          std::vector<unsigned> tabPositions;
          tabPositions.resize(size_t(nTabs));
          for (auto &pos:tabPositions) pos=readU16(input);
          for (size_t t=0; t<nTabs; ++t)
          {
            TabStop tab(tabPositions[t]*635);
            auto fl=readU8(input);
            switch (fl&3)
            {
            case 0: // LEFT
            default:
              break;
            case 1:
              tab.m_alignment=tab.CENTER;
              break;
            case 2:
              tab.m_alignment=tab.RIGHT;
              break;
            case 3:
              tab.m_alignment=tab.DECIMAL;
              break;
            }
            switch ((fl>>3)&3)
            {
            case 0: // none
            default:
              break;
            case 1:
              tab.m_leaderChar='.';
              break;
            case 2:
              tab.m_leaderChar='-';
              break;
            case 3:
              tab.m_leaderChar='_';
              break;
            }
            if ((fl&0xe4)!=0x80)
            {
              MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: find unexpected flags=%x\n", fl));
            }
            style.m_tabStops.push_back(tab);
          }
        }
      }
    }
    unsigned newId=styles.size();
    posToStyle[positions[i]]=newId;
    offsetToStyleMap[offs]=newId;
    styles.push_back(style);
  }
  return true;
}

bool MSPUBParser97::parseSpanStyles(librevenge::RVNGInputStream *input, unsigned index,
                                    std::vector<CharacterStyle> &styles,
                                    std::map<unsigned, unsigned> &posToStyle)
{
  if (input->seek((index+1)*0x200-1, librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseSpanStyles: can not use index=%x\n", index));
    return false;
  }
  unsigned N=unsigned(readU8(input));
  if ((N+1)*5>0x200)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseSpanStyles: N value=%d is too big for index=%x\n", N, index));
    return false;
  }
  input->seek(index*0x200, librevenge::RVNG_SEEK_SET);
  std::vector<unsigned> positions;
  positions.resize(N+1);
  for (auto &p : positions) p=readU32(input);
  std::vector<uint8_t> stylePos;
  stylePos.reserve(N);
  for (unsigned i=0; i<N; ++i) stylePos.push_back(readU8(input));
  if (styles.empty())
    styles.push_back(CharacterStyle());
  std::map<unsigned, unsigned> offsetToStyleMap;
  offsetToStyleMap[0]=0;
  for (unsigned i=0; i<N; ++i)
  {
    auto const &offs=stylePos[i];
    auto oIt=offsetToStyleMap.find(offs);
    if (oIt!=offsetToStyleMap.end())
    {
      posToStyle[positions[i]]=oIt->second;
      continue;
    }
    input->seek(index*0x200+2*offs, librevenge::RVNG_SEEK_SET);
    uint8_t len=readU8(input);
    if (len==0 || 2*offs+1+len>0x200)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseSpanStyles: can not read len for i=%x for index=%x\n", i, index));
      posToStyle[positions[i]]=0;
      continue;
    }

    unsigned newId=styles.size();
    posToStyle[positions[i]]=newId;
    offsetToStyleMap[offs]=newId;
    styles.push_back(readCharacterStyle(input, len));
  }
  return true;
}

CharacterStyle MSPUBParser97::readCharacterStyle(
  librevenge::RVNGInputStream *input, unsigned length)
{
  CharacterStyle style;

  unsigned begin = input->tell();
  int textSizeVariationFromDefault = 0;

  if (length >= 1)
  {
    unsigned char biFlags = readU8(input);
    style.bold = biFlags & 0x1;
    style.italic = biFlags & 0x2;
    style.smallCaps = biFlags & 0x4;
    style.allCaps = biFlags & 0x8;
  }
  if (length >= 3)
  {
    input->seek(begin + 0x2, librevenge::RVNG_SEEK_SET);
    style.fontIndex = readU8(input);
  }
  if (length >= 5)
  {
    input->seek(begin + 0x4, librevenge::RVNG_SEEK_SET);
    textSizeVariationFromDefault =
      length >= 6 ? readS16(input) : readS8(input);
  }
  if (length >= 7)
  {
    int baseLine=readS8(input);
    style.superSubType = baseLine<0 ? SUBSCRIPT : baseLine>0 ? SUPERSCRIPT : NO_SUPER_SUB;
  }
  if (m_version<=2)
  {
    if (length >= 8)
      style.colorIndex = getColorIndexByQuillEntry(readU8(input));
    if (length >= 9)
    {
      unsigned fl=length>=10 ? readU16(input) : readU8(input);
      switch (fl&3)
      {
      case 0: // none
        break;
      case 1:
      case 2:
        style.underline = Underline::Single;
        break;
      case 3:
        style.underline = Underline::Double;
        break;
      }
      auto spacing=(fl>>2)&0x1fff;
      if (spacing&0x1000)
        style.letterSpacingInPt=(double(spacing)-0x2000)/8;
      else if (spacing)
        style.letterSpacingInPt=double(spacing)/8;
    }
  }
  else if (length >= 16)
  {
    input->seek(begin + 0xC, librevenge::RVNG_SEEK_SET);
    style.colorIndex = getColorIndexByQuillEntry(readU32(input));
  }
  style.textSizeInPt = 10 +
                       static_cast<double>(textSizeVariationFromDefault) / 2;

  return style;
}

void MSPUBParser97::getTextInfo(librevenge::RVNGInputStream *input, unsigned length, std::map<unsigned,MSPUBParser97::What> &posToType)
{
  length = std::min(length, m_length); // sanity check
  unsigned start = input->tell();
  unsigned char last = '\0';
  unsigned pos=0;
  while (stillReading(input, start + length))
  {
    unsigned char ch=readU8(input);
    if (last == 0xD && ch == 0xA)
      posToType[pos]=LineEnd;
    else if (ch == 0xC)
      posToType[pos]=ShapeEnd;
    else if (ch == 0xF)
      posToType[pos]=CellEnd;
    else if (last=='#' && ch==0x5)
      posToType[pos-1]=FieldBegin;
    last = ch;
    ++pos;
  }
}

unsigned MSPUBParser97::getFirstLineOffset() const
{
  return 0x22;
}

unsigned MSPUBParser97::getSecondLineOffset() const
{
  return 0x2D;
}

unsigned MSPUBParser97::getShapeFillTypeOffset() const
{
  return 0x20;
}

unsigned MSPUBParser97::getShapeFillColorOffset() const
{
  return 0x18;
}

void MSPUBParser97::parseShapeFormat(librevenge::RVNGInputStream *input, unsigned seqNum,
                                     ChunkHeader2k const &header)
{
  if (m_version>2)
    return MSPUBParser2k::parseShapeFormat(input, seqNum, header);

  if (header.m_type==C_Group)
    return;
  if (input->tell()+9>header.m_dataOffset)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseShapeFormat: the zone is too small\n"));
    return;
  }
  input->seek(2, librevenge::RVNG_SEEK_CUR); // flags: 40=selected, 1=shadow
  int colors[2];
  for (auto &c : colors) c=int(readU8(input));
  int patternId=int(readU8(input));
  int numBorders=1;
  int bColors[4];
  double widths[4];
  bColors[0]=int(readU8(input));
  auto w=readU8(input);
  widths[0]=(w&0x80) ? double(w&0x7f)/4 : double(w);
  input->seek(2,librevenge::RVNG_SEEK_CUR); // 0
  int borderId=0xfffe; // none
  if (header.isRectangle())
  {
    borderId=int(readU16(input));
    input->seek(1,librevenge::RVNG_SEEK_CUR); // a width
    numBorders=4;
    for (int j=1; j<4; ++j)
    {
      bColors[j]=int(readU8(input));
      w=readU8(input);
      widths[j]=(w&0x80) ? double(w&0x7f)/4 : double(w);
    }
    if (header.m_type == C_Text)
    {
      input->seek(8, librevenge::RVNG_SEEK_CUR); // margin?
      unsigned txtId = readU16(input);
      m_collector->addTextShape(txtId, seqNum);
    }
    else if (header.m_type == C_Table)
    {
      input->seek(8, librevenge::RVNG_SEEK_CUR); // margin?
      m_collector->addTextShape(readU16(input), seqNum);
      input->seek(2, librevenge::RVNG_SEEK_CUR); // data size ?
      unsigned numCols = readU16(input);
      unsigned numRows = readU16(input);
      unsigned width=readU32(input);
      unsigned height=readU32(input);
      if (numRows && numCols)
      {
        TableInfo ti(numRows, numCols);
        // FIXME
        std::vector<unsigned> rowHeightsInEmu(size_t(numRows),height/numRows);
        std::vector<unsigned> columnWidthsInEmu(size_t(numCols),width/numCols);
        ti.m_rowHeightsInEmu = rowHeightsInEmu;
        ti.m_columnWidthsInEmu = columnWidthsInEmu;
        for (unsigned r=0; r<numRows; r++)
        {
          CellInfo cellInfo;
          cellInfo.m_startRow=cellInfo.m_endRow=r;
          for (unsigned c=0; c<numCols; c++)
          {
            cellInfo.m_startColumn=cellInfo.m_endColumn=c;
            ti.m_cells.push_back(cellInfo);
          }
        }
        m_collector->setShapeTableInfo(seqNum, ti);
      }
    }
  }
  else if (header.m_type==C_CustomShape)
  {
    ShapeType shapeType = getShapeType(readU16(input));
    if (shapeType != UNKNOWN_SHAPE)
    {
      m_collector->setShapeType(seqNum, shapeType);
      /*
      for (int i=0; i<4; ++i)
        m_collector->setAdjustValue(seqNum, i, readS16(input));
      */
    }
    auto flags=readU16(input);
    if (flags&3)
      m_collector->setShapeFlip(seqNum, flags&1, flags&2);
    int rot=(flags>>2)&3;
    if (rot)
      m_collector->setShapeRotation(seqNum,360-90*rot);
  }
  else if (header.m_type==C_Line)
  {
    input->seek(16, librevenge::RVNG_SEEK_CUR);
    auto flags=readU16(input);
    if ((flags&0x1)==0)
      m_collector->setShapeFlip(seqNum, true, false);
    if (flags&0x6)
    {
      static Arrow const arrows[]=
      {
        Arrow(NO_ARROW, MEDIUM, LARGE),
        Arrow(TRIANGLE_ARROW, MEDIUM, LARGE),
        Arrow(TRIANGLE_ARROW, MEDIUM, MEDIUM),
        Arrow(TRIANGLE_ARROW, MEDIUM, SMALL),
        Arrow(TRIANGLE_ARROW, MEDIUM, LARGE), // 4:UNKNOWN
        Arrow(LINE_ARROW, MEDIUM, MEDIUM),
        Arrow(TRIANGLE_ARROW, MEDIUM, LARGE), // 6:UNKNOWN
        Arrow(LINE_ARROW, MEDIUM, SMALL),
        Arrow(KITE_ARROW, MEDIUM, LARGE),
        Arrow(KITE_ARROW, MEDIUM, MEDIUM),
        Arrow(ROTATED_SQUARE_ARROW, MEDIUM, MEDIUM),
        Arrow(TRIANGLE1_ARROW, MEDIUM, MEDIUM),
        Arrow(TRIANGLE_ARROW, MEDIUM, LARGE), // c:UNKNOWN
        Arrow(TRIANGLE1_ARROW, MEDIUM, SMALL),
        Arrow(TRIANGLE_ARROW, MEDIUM, LARGE), // e:UNKNOWN
        Arrow(FAT_LINE_ARROW, MEDIUM, MEDIUM),
        Arrow(FAT_LINE_ARROW, MEDIUM, SMALL),
        Arrow(BLOCK_ARROW, MEDIUM, LARGE),
        Arrow(TRIANGLE_ARROW, MEDIUM, LARGE), // 12:UNKNOWN
        Arrow(TRIANGLE_ARROW, MEDIUM, LARGE), // 13:UNKNOWN
        Arrow(TRIANGLE2_ARROW, MEDIUM, MEDIUM),
      };
      int numArrows=int(MSPUB_N_ELEMENTS(arrows));
      int begArrow=((flags>>4)&0x1f);
      if (begArrow>=numArrows) begArrow=1;
      int endArrow=((flags>>9)&0x1f);
      if (endArrow>=numArrows) endArrow=0;
      if (flags&0x2)
      {
        m_collector->setShapeEndArrow(seqNum, arrows[begArrow]);
        if (endArrow && (flags&0x4)==0)
        {
          Arrow finalArrow=arrows[endArrow];
          finalArrow.m_flipY=true;
          m_collector->setShapeBeginArrow(seqNum, finalArrow);
        }
      }
      if (flags&0x4)
      {
        m_collector->setShapeBeginArrow(seqNum, arrows[begArrow]);
        if (endArrow && (flags&0x2)==0)
        {
          Arrow finalArrow=arrows[endArrow];
          finalArrow.m_flipY=true;
          m_collector->setShapeEndArrow(seqNum, finalArrow);
        }
      }
    }
  }
  if (borderId>=0x8000)
  {
    for (int i=0; i<numBorders; ++i)
    {
      int wh=i==0 ? numBorders-1 : i-1;
      Color lineColor=getColorBy2kIndex(bColors[wh]&0xf);
      double delta=double((bColors[wh]>>5)&3)/4; // pattern 0: means line color, 3: means 1/4 of color 3/4 of white
      unsigned rgb[]=
      {
        unsigned((1-delta)*double(lineColor.r)+delta*255),
        unsigned((1-delta)*double(lineColor.g)+delta*255),
        unsigned((1-delta)*double(lineColor.b)+delta*255)
      };
      m_collector->addShapeLine(seqNum, Line(ColorReference(rgb[0]|(rgb[1]<<8)|(rgb[2]<<16)), widths[wh]*12700, widths[wh]>0));
    }
  }
  else if (widths[0]>0)
  {
    Color lineColor=getColorBy2kIndex(bColors[0]&0xf);
    double delta=double((bColors[0]>>5)&3)/4; // pattern 0: means line color, 3: means 1/4 of color 3/4 of white
    unsigned rgb[]=
    {
      unsigned((1-delta)*double(lineColor.r)+delta*255),
      unsigned((1-delta)*double(lineColor.g)+delta*255),
      unsigned((1-delta)*double(lineColor.b)+delta*255)
    };
    m_collector->addShapeLine(seqNum, Line(ColorReference(rgb[0]|(rgb[1]<<8)|(rgb[2]<<16)), widths[0]*12700, true));
    m_collector->setShapeBorderImageId(seqNum, unsigned(borderId));
    m_collector->setShapeBorderPosition(seqNum, OUTSIDE_SHAPE);
  }
  if (patternId)
  {
    if (patternId==1 || patternId==2)
      m_collector->setShapeFill(seqNum, std::make_shared<SolidFill>(getColorReferenceBy2kIndex(colors[2-patternId]), 1, m_collector), false);
    else if (patternId>=3 && patternId<=24)
    {
      uint8_t const patterns[]=
      {
        0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd,
        0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
        0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22,
        0x00, 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22,
        0x08, 0x00, 0x80, 0x00, 0x08, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x80,
        0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0x33, 0x33,
        0x88, 0x88, 0x88, 0xff, 0x88, 0x88, 0x88, 0xff,
        0x88, 0x55, 0x22, 0x55, 0x88, 0x55, 0x22, 0x55,
        0x11, 0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22,
        0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x33, 0x99, 0xcc, 0x66, 0x33, 0x99, 0xcc, 0x66,
        0x33, 0x66, 0xcc, 0x99, 0x33, 0x66, 0xcc, 0x99,
        0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
        0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
        0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x55, 0x00,
        0xff, 0x01, 0x01, 0x01, 0xff, 0x10, 0x10, 0x10,
        0x01, 0x02, 0x04, 0x08, 0x14, 0x22, 0x41, 0x80,
        0x11, 0xa2, 0x44, 0x2a, 0x11, 0x8a, 0x44, 0xa8
      };

      m_collector->setShapeFill(seqNum, std::make_shared<Pattern88Fill>
                                (m_collector,(uint8_t const(&)[8])(patterns[8*(patternId-3)]),getColorReferenceBy2kIndex(colors[1]), getColorReferenceBy2kIndex(colors[0])), false);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseShapeFormat: unknown pattern =%d\n", patternId));
    }
  }
}
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
