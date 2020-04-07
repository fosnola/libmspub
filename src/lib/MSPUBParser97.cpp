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
#include <utility>

#include "MSPUBCollector.h"
#include "MSPUBTypes.h"
#include "TableInfo.h"
#include "libmspub_utils.h"

namespace libmspub
{

MSPUBParser97::MSPUBParser97(librevenge::RVNGInputStream *input, MSPUBCollector *collector)
  : MSPUBParser2k(input, collector)
  , m_bulletLists()
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


void MSPUBParser97::parseBulletDefinitions(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input)
{
  ChunkHeader2k header;
  parseChunkHeader(chunk,input,header);
  ListHeader2k listHeader;
  if (!header.hasData())
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseBulletDefinitions: can not find the data zone\n"));
    return;
  }
  input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
  if (!parseListHeader(input, chunk.end, listHeader, false) || listHeader.m_dataSize!=5)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseBulletDefinitions: can not read the data zone\n"));
    return;
  }
  m_bulletLists.reserve(size_t(listHeader.m_N));
  for (int id=0; id<listHeader.m_N; ++id)
  {
    unsigned char c=readU8(input);
    int fontSize=int(readU8(input));
    input->seek(1+2, librevenge::RVNG_SEEK_CUR); // 0 and font index name: not read
    // assume default symbol font
    static unsigned int const symbolLow[]= // 0x20-0x7f
    {
      0x0020, 0x0021, 0x2200, 0x0023, 0x2203, 0x0025, 0x0026, 0x220D,
      0x0028, 0x0029, 0x2217, 0x002B, 0x002C, 0x2212, 0x002E, 0x002F,
      0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
      0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
      0x2245, 0x0391, 0x0392, 0x03A7, 0x0394, 0x0395, 0x03A6, 0x0393,
      0x0397, 0x0399, 0x03D1, 0x039A, 0x039B, 0x039C, 0x039D, 0x039F,
      0x03A0, 0x0398, 0x03A1, 0x03A3, 0x03A4, 0x03A5, 0x03C2, 0x03A9,
      0x039E, 0x03A8, 0x0396, 0x005B, 0x2234, 0x005D, 0x22A5, 0x005F,
      0xF8E5, 0x03B1, 0x03B2, 0x03C7, 0x03B4, 0x03B5, 0x03C6, 0x03B3,
      0x03B7, 0x03B9, 0x03D5, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BF,
      0x03C0, 0x03B8, 0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03D6, 0x03C9,
      0x03BE, 0x03C8, 0x03B6, 0x007B, 0x007C, 0x007D, 0x223C, 0x007f
    };
    static unsigned int const symbolHigh[]= // 0xa0-0xff
    {
      0x20AC, 0x03D2, 0x2032, 0x2264, 0x2044, 0x221E, 0x0192, 0x2663,
      0x2666, 0x2665, 0x2660, 0x2194, 0x2190, 0x2191, 0x2192, 0x2193,
      0x00B0, 0x00B1, 0x2033, 0x2265, 0x00D7, 0x221D, 0x2202, 0x2022,
      0x00F7, 0x2260, 0x2261, 0x2248, 0x2026, 0x23D0, 0x23AF, 0x21B5,
      0x2135, 0x2111, 0x211C, 0x2118, 0x2297, 0x2295, 0x2205, 0x2229,
      0x222A, 0x2283, 0x2287, 0x2284, 0x2282, 0x2286, 0x2208, 0x2209,
      0x2220, 0x2207, 0x00AE, 0x00A9, 0x2122, 0x220F, 0x221A, 0x22C5,
      0x00AC, 0x2227, 0x2228, 0x21D4, 0x21D0, 0x21D1, 0x21D2, 0x21D3,
      0x25CA, 0x3008, 0x00AE, 0x00A9, 0x2122, 0x2211, 0x239B, 0x239C,
      0x239D, 0x23A1, 0x23A2, 0x23A3, 0x23A7, 0x23A8, 0x23A9, 0x23AA,
      0xF8FF, 0x3009, 0x222B, 0x2320, 0x23AE, 0x2321, 0x239E, 0x239F,
      0x23A0, 0x23A4, 0x23A5, 0x23A6, 0x23AB, 0x23AC, 0x23AD, 0x00FF
    };
    const unsigned char cl = c & 0x7f;
    unsigned unicode = cl<0x20 ? 0x2022 : (c&0x80) ? symbolHigh[cl - 0x20] : symbolLow[cl - 0x20];
    m_bulletLists.push_back(ListInfo(unicode));
    if (fontSize>1) m_bulletLists.back().m_fontSize=double(fontSize)/2;
  }
}

void MSPUBParser97::parseContentsTextIfNecessary(librevenge::RVNGInputStream *input)
{
  input->seek(0x12, librevenge::RVNG_SEEK_SET);
  unsigned blockStart=readU32(input);
  input->seek(blockStart+4, librevenge::RVNG_SEEK_SET);
  unsigned version=readU16(input);
  if (version>=200 && version<300) m_version=2; // mspub 2
  else if (version>=300)
  {
    // let read the content major sub version
    input->seek(7, librevenge::RVNG_SEEK_SET);
    unsigned subVersion=readU8(input);
    if (subVersion==0 || subVersion==1)
      m_version=3+subVersion;
    else   // ok assume 97
    {
      m_version=4;
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseContentsTextIfNecessary: oops find unknown sub version=%d, assume 97\n", int(subVersion)));
    }
  }
  else   // assume mspub 2
  {
    m_version=2;
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseContentsTextIfNecessary: oops find version=%d, assume v2\n", int(version)));
  }
  if (m_version<=3)
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
  std::vector<CellStyle> cellStyles;
  std::map<unsigned, unsigned> posToCellMap;
  if (m_version<=3)
  {
    for (unsigned id=index[1]; id<index[2]; ++id)
      parseParagraphStyles(input, id, paraStyles, posToParaMap);
    for (unsigned id=index[2]; id<index[3]; ++id)
      parseCellStyles(input, id, cellStyles, posToCellMap);
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

  size_t oldParaPos=0; // used to check for empty line
  size_t actChar=0;
  std::vector<CellStyle> cellStyleList;
  for (unsigned c=0; c<length; ++c)
  {
    // change of style
    auto actPos=input->tell();
    auto cIt=posToSpanMap.find((unsigned) actPos);
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
    auto pIt=posToParaMap.find((unsigned) actPos);
    if (pIt!=posToParaMap.end())
    {
      if (pIt->second<paraStyles.size())
        paraStyle=paraStyles[pIt->second];
      else
        paraStyle=ParagraphStyle();
    }
    auto cellIt=posToCellMap.find((unsigned) actPos);
    if (cellIt!=posToCellMap.end())
    {
      if (cellIt->second<=cellStyles.size())
        cellStyleList.push_back(cellStyles[cellIt->second]);
      else
        cellStyleList.push_back(CellStyle());
    }
    unsigned char ch=readU8(input);
    // special character
    auto specialIt=posToTypeMap.find(c);
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
        if (m_version==2)
        {
          input->seek(1, librevenge::RVNG_SEEK_CUR);
          ++c;
          ch=readU8(input);
        }
        if (ch==0x5)
        {
          TextSpan pageSpan(spanChars,charStyle);
          pageSpan.field=Field(Field::PageNumber);
          paraSpans.push_back(pageSpan);
        }
        else if (ch==0x6 && charStyle.fieldId)
        {
          char const *(dtFormat[])=
          {
            "%m/%d/%y", "%A, %B %d, %Y", "%d/%m/%y", "%A, %B %d, %Y",
            "%d %B, %Y", "%B %d, %Y", "%d-%b-%y", "%B, %y", "%b-%y",
            "%m/%d/%y %I:%M %p", "%m/%d/%y %I:%M:%S %p",
            "%I:%M %p", "%I:%M:%S %p", "%I:%M", "%I:%M:%S", "%H:%M", "%H:%M:%S",
          };
          if (*charStyle.fieldId>=1 && *charStyle.fieldId<=MSPUB_N_ELEMENTS(dtFormat))
          {
            TextSpan dtSpan(spanChars,charStyle);
            dtSpan.field=Field(*charStyle.fieldId<12 ? Field::Date : Field::Time);
            dtSpan.field->m_DTFormat=dtFormat[*charStyle.fieldId-1];
            paraSpans.push_back(dtSpan);
          }
          else
          {
            MSPUB_DEBUG_MSG(("MSPUBParser97::parseContentsTextIfNecessary: unknown a date field=%d\n",*charStyle.fieldId));
          }
        }
        else
        {
          MSPUB_DEBUG_MSG(("MSPUBParser97::parseContentsTextIfNecessary: field %x is not implemented\n",unsigned(ch)));
        }
        continue;
      }
      bool needNewPara=!paraSpans.empty();
      if (!needNewPara && oldParaPos+2>=unsigned(actPos) && specialIt->second==LineEnd)
      {
        auto sIt=specialIt;
        if (sIt!=posToTypeMap.end() && sIt->second!=ShapeEnd) ++sIt;
        needNewPara=sIt==posToTypeMap.end() || sIt->second!=ShapeEnd;
      }
      if (needNewPara)
      {
        shapeParas.push_back(TextParagraph(paraSpans, paraStyle));
        paraSpans.clear();
      }
      oldParaPos=unsigned(actPos);
      if (specialIt->second==CellEnd)
        cellEnds.push_back(unsigned(actChar)+1); // offset begin at 1...
      if (specialIt->second==ShapeEnd)
      {
        if (!cellEnds.empty())
        {
          m_collector->setTableCellTextEnds(shape,cellEnds);
          cellEnds.clear();
        }
        if (cellStyleList.size()>1)
          m_collector->setTableCellTextStyles(shape, cellStyleList);
        m_collector->addTextString(shapeParas, shape++);
        charStyle=CharacterStyle();
        shapeParas.clear();
        cellStyleList.clear();
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
    if (cellStyleList.size()>1)
      m_collector->setTableCellTextStyles(shape, cellStyleList);
    m_collector->addTextString(shapeParas, shape++);
  }
}

bool MSPUBParser97::parseCellStyles(librevenge::RVNGInputStream *input, unsigned index,
                                    std::vector<CellStyle> &styles,
                                    std::map<unsigned, unsigned> &posToStyle)
{
  if (input->seek((index+1)*0x200-1, librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseCellStyles: can not use index=%x\n", index));
    return false;
  }
  unsigned N=unsigned(readU8(input));
  if ((N+1)*5>0x200)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseCellStyles: N value=%d is too big for index=%x\n", N, index));
    return false;
  }
  input->seek(index*0x200+4, librevenge::RVNG_SEEK_SET);
  std::vector<unsigned> positions;
  positions.resize(N);
  for (auto &p : positions) p=readU32(input);
  std::vector<uint8_t> stylePos;
  stylePos.reserve(N);
  for (unsigned i=0; i<N; ++i) stylePos.push_back(readU8(input));
  if (styles.empty())
    styles.push_back(CellStyle());
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
    long begPos=index*0x200+2*offs;
    input->seek(begPos, librevenge::RVNG_SEEK_SET);
    uint8_t len=readU8(input);
    if (len==0 || 2*offs+1+len>0x200)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseCellStyles: can not read len for i=%x for index=%x\n", i, index));
      posToStyle[positions[i]]=0;
      continue;
    }
    auto endPos=begPos+1+len;
    unsigned newId=unsigned(styles.size());
    posToStyle[positions[i]]=newId;
    offsetToStyleMap[offs]=newId;
    styles.push_back(CellStyle());
    auto &style=styles.back();
    input->seek(2, librevenge::RVNG_SEEK_CUR);
    // the surface color
    unsigned bgColors[2];
    bool ok=true;
    for (int j=0; j<2; ++j)
    {
      if (input->tell()+(m_version==2 ? 1 : 4)>endPos)
      {
        ok=false;
        break;
      }
      bgColors[j]=m_version<=2 ? readU8(input) : readU32(input);
    }
    if (!ok || input->tell()>=endPos) continue;
    int patternId=int(readU8(input));
    unsigned bgRefColors[2];
    for (int j=0; j<2; ++j) bgRefColors[j]=translate2kColorReference(bgColors[j]);
    if (patternId==1 || patternId==2)
      style.m_color=ColorReference(bgRefColors[2-patternId]);
    else if (patternId)
    {
      double percent=-1;
      if (patternId&0x80) // gradient, create medium color
        percent=0.5;
      else if (patternId>=3 && patternId<=24)
      {
        double const percents[]= {0.5, 0.5, 0.25, 0.125,
                                  0.0625, 0.03125, 0.5, 0.43,
                                  0.375, 0.25, 0.25, 025,
                                  0.5, 0.5, 0.5, 0.25,
                                  0.5, 0.094, 0.43, 0.125,
                                  0.32
                                 };
        percent=percents[patternId-3];
      }
      else
      {
        MSPUB_DEBUG_MSG(("MSPUBParser97::parseShapeFormat: unknown pattern=%d\n", patternId));
      }
      if (percent>0)
      {
        unsigned finalColor=0, depl=0;
        for (int c=0; c<3; ++c, depl+=8)
        {
          finalColor|=unsigned(percent*double((bgRefColors[1]>>depl)&0xff)+
                               (1-percent)*double((bgRefColors[0]>>depl)&0xff))<<depl;
        }
        style.m_color=ColorReference(finalColor);
      }
    }
    input->seek(m_version==2 ? 9 : 8, librevenge::RVNG_SEEK_CUR); // margins + unknown
    // the border
    for (int b=0; b<4; ++b)
    {
      if (m_version>=3) input->seek(1, librevenge::RVNG_SEEK_CUR); // type
      if (input->tell()>=endPos) break;
      auto w=readU8(input);
      double width=(w&0x80) ? double(w&0x7f)/4 : double(w);
      unsigned color=0;
      if (input->tell()+(m_version==2 ? 1 : 4)<=endPos)
        color=m_version==2 ? readU8(input) : readU32(input);
      style.m_borders.push_back(Line(ColorReference(translate2kColorReference(color)), unsigned(width*12700), width>0));
    }
  }
  return true;
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
  input->seek(index*0x200+4, librevenge::RVNG_SEEK_SET);
  std::vector<unsigned> positions;
  positions.resize(N);
  for (auto &p : positions) p=readU32(input)-1;
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
    long debPos=index*0x200+2*offs;
    input->seek(debPos, librevenge::RVNG_SEEK_SET); // skip small size
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
      switch (align&3)
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
        break;
      }
      int spacing=(align>>3)&0xf;
      if (spacing>=1 && spacing<=4)
      {
        double values[]= {0, -1, -0.5, 1.5, 3};
        style.m_letterSpacingInPt=values[spacing];
      }
      else if (spacing)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: unknown spacing=%d\n", spacing));
      }
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
    if (tabPos>=33)
    {
      input->seek(debPos+32, librevenge::RVNG_SEEK_SET);
      auto numberId=readU8(input);
      if (numberId>=1 && numberId<=3)
      {
        NumberingType const(types[])= { STANDARD_WESTERN, LOWERCASE_LETTERS, UPPERCASE_LETTERS};
        input->seek(1, librevenge::RVNG_SEEK_CUR); // 0|1
        boost::optional<unsigned> numberIfRestarted;
        if (tabPos>=34) numberIfRestarted=tabPos>=35 ? readU16(input) : readU8(input);

        // CHANGE: converting delimiters string in enum then back in string is not optimal...
        char delimiters[]= {0,0};
        if (tabPos>=36) delimiters[0]=char(readU8(input));
        if (tabPos>=37) delimiters[1]=char(readU8(input));
        static std::map<std::pair<char,char>,NumberingDelimiter> const delimiterMaps=
        {
          { {0,0}, NO_DELIMITER},
          { {0,')'}, PARENTHESIS},
          { {'(',')'}, PARENTHESES_SURROUND},
          { {0,'.'}, PERIOD},
          { {0,']'}, SQUARE_BRACKET},
          { {0,':'}, COLON},
          { {'[',']'}, SQUARE_BRACKET_SURROUND},
          { {'-','-'}, HYPHEN_SURROUND},
        };
        auto delIt=delimiterMaps.find({delimiters[0],delimiters[1]});
        if (delIt==delimiterMaps.end())
        {
          MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: unknown delimiters=%x:%x", unsigned(delimiters[0]),unsigned(delimiters[1])));
        }
        style.m_listInfo=ListInfo(numberIfRestarted, types[numberId-1], delIt==delimiterMaps.end() ? NO_DELIMITER : delIt->second);
      }
      else if (numberId>0)
      {
        if (numberId>=10 && numberId<10+int(m_bulletLists.size()))
          style.m_listInfo=m_bulletLists[size_t(numberId-10)];
        else
        {
          MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: unknown type=%d\n", numberId));
          style.m_listInfo=ListInfo(0x2022);
        }
      }
    }
    if (m_version>=3 && tabPos>=39)
    {
      input->seek(debPos+38, librevenge::RVNG_SEEK_SET);
      unsigned fancy=readU16(input);
      if (fancy)
      {
        style.m_dropCapLines=2;
        style.m_dropCapLetters=1;
        MSPUB_DEBUG_MSG(("MSPUBParser97::parseParagraphStyles: find fancy character=%x, ignored\n", fancy));
      }
    }
    if (1+tabPos+3<2*len+1)
    {
      input->seek(debPos+1+tabPos, librevenge::RVNG_SEEK_SET);
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
    unsigned newId=unsigned(styles.size());
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

    unsigned newId=unsigned(styles.size());
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

  unsigned begin = unsigned(input->tell());
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
  if (length >= 8)
  {
    if (m_version<3)
      style.colorIndex = int(getColorIndexByQuillEntry(readU8(input)));
    else // color is no longer stored here
      input->seek(1, librevenge::RVNG_SEEK_CUR);
  }
  if (m_version<=3 && length >= 9)
  {
    unsigned fl=length>=10 ? readU16(input) : readU8(input);
    switch (fl&3)
    {
    case 0: // none
    default:
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
  if (m_version>=3 && length >= 11)
    style.fieldId=readU8(input);
  if (m_version>=3 && length >= 12)
  {
    input->seek(begin + 0xC, librevenge::RVNG_SEEK_SET);
    style.colorIndex = int(getColorIndexByQuillEntry(length<14 ? readU8(input) : length<16 ? readU16(input) : readU32(input)));
  }
  style.textSizeInPt = 10 +
                       static_cast<double>(textSizeVariationFromDefault) / 2;

  return style;
}

void MSPUBParser97::getTextInfo(librevenge::RVNGInputStream *input, unsigned length, std::map<unsigned,MSPUBParser97::What> &posToType)
{
  length = std::min(length, m_length); // sanity check
  unsigned start = unsigned(input->tell());
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
    else if (ch==0x5)
    {
      if (m_version==2 && last=='#')
        posToType[pos-1]=FieldBegin;
      else if (m_version>=3)
        posToType[pos]=FieldBegin;
    }
    else if (ch==0x6 && m_version>=3)
      posToType[pos]=FieldBegin;
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

void MSPUBParser97::parseTableInfoData(librevenge::RVNGInputStream *input, unsigned seqNum, ChunkHeader2k const &header,
                                       unsigned numCols, unsigned numRows, unsigned width, unsigned height)
{
  if (!numRows || !numCols || numRows>128 || numCols>128)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseTableInfoData: unexpected number of rows/columns\n"));
    return;
  }

  TableInfo ti(numRows, numCols);
  if (header.hasData())
  {
    input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
    ListHeader2k listHeader;
    if (!parseListHeader(input, header.m_endOffset, listHeader, false) || listHeader.m_dataSize!=14 || listHeader.m_N<int(numCols+numRows))
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseTableInfoData: can not read the data zone\n"));
    }
    else
    {
      for (int wh=0; wh<2; ++wh)
      {
        unsigned num=wh==0 ? numCols : numRows;
        std::vector<unsigned> &sizes=wh==0 ? ti.m_columnWidthsInEmu : ti.m_rowHeightsInEmu;
        sizes.reserve(size_t(num));
        unsigned actPos=0;
        for (unsigned i=0; i<num; ++i)
        {
          unsigned newPos=readU32(input);
          if (newPos<actPos)
          {
            MSPUB_DEBUG_MSG(("MSPUBParser97::parseTableInfoData: oops a position is bad\n"));
            sizes.push_back((wh==0 ? width : height)/num);
          }
          else
          {
            sizes.push_back(newPos-actPos);
            actPos=newPos;
          }
          input->seek(10, librevenge::RVNG_SEEK_CUR);
        }
      }
    }
  }
  else
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseTableInfoData: can not find the data zone\n"));
  }
  // just in case we have no read all row/columns
  ti.m_rowHeightsInEmu.resize(size_t(numRows),height/numRows);
  ti.m_columnWidthsInEmu.resize(size_t(numCols),width/numCols);
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

void MSPUBParser97::parseShapeFormat(librevenge::RVNGInputStream *input, unsigned seqNum,
                                     ChunkHeader2k const &header)
{
  if (m_version>3)
    return MSPUBParser2k::parseShapeFormat(input, seqNum, header);

  if (header.m_type==C_Group)
    return;
  if (input->tell()+(m_version==2 ? 9 : 19)>header.m_dataOffset)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser97::parseShapeFormat: the zone is too small\n"));
    return;
  }
  input->seek(2, librevenge::RVNG_SEEK_CUR); // flags: 40=selected, 1=shadow
  unsigned colors[2];
  for (auto &c : colors) c=m_version<=2 ? readU8(input) : readU32(input);
  int patternId=int(readU8(input));
  if (m_version>=3) input->seek(1, librevenge::RVNG_SEEK_CUR);
  int numBorders=1;
  unsigned bColors[4];
  double widths[4];
  if (m_version<=2)
  {
    bColors[0]=readU8(input);
    auto w=readU8(input);
    widths[0]=(w&0x80) ? double(w&0x7f)/4 : double(w);
  }
  else
  {
    auto w=readU8(input);
    widths[0]=(w&0x80) ? double(w&0x7f)/4 : double(w);
    bColors[0]=readU32(input);
  }
  input->seek(2,librevenge::RVNG_SEEK_CUR); // 0
  int borderId=0xfffe; // none
  if (header.isRectangle() && input->tell()+(m_version==2 ? 9 : 21)<=header.m_dataOffset)
  {
    borderId=int(readU16(input));
    input->seek(1,librevenge::RVNG_SEEK_CUR); // a width
    numBorders=4;
    if (m_version<=2)
    {
      for (int j=1; j<4; ++j)
      {
        bColors[j]=readU8(input);
        auto w=readU8(input);
        widths[j]=(w&0x80) ? double(w&0x7f)/4 : double(w);
      }
    }
    else
    {
      for (int j=1; j<4; ++j)
      {
        input->seek(1,librevenge::RVNG_SEEK_CUR);
        auto w=readU8(input);
        widths[j]=(w&0x80) ? double(w&0x7f)/4 : double(w);
        bColors[j]=readU32(input);
      }
    }
    if (header.m_type == C_Text && input->tell()+10<=header.m_dataOffset)
    {
      input->seek(8, librevenge::RVNG_SEEK_CUR); // margin?
      unsigned txtId = readU16(input);
      m_collector->addTextShape(txtId, seqNum);
    }
    else if (header.m_type == C_Table && input->tell()+(m_version==2 ? 24 : 32)<=header.m_dataOffset)
    {
      input->seek(8, librevenge::RVNG_SEEK_CUR); // margin?
      m_collector->addTextShape(readU16(input), seqNum);
      input->seek(2, librevenge::RVNG_SEEK_CUR); // data size ?
      unsigned numCols = readU16(input);
      if (m_version>2) input->seek(4, librevenge::RVNG_SEEK_CUR); // 0?
      unsigned numRows = readU16(input);
      if (m_version>2) input->seek(4, librevenge::RVNG_SEEK_CUR); // 0?
      unsigned width=readU32(input);
      unsigned height=readU32(input);
      if (numRows && numCols)
        parseTableInfoData(input, seqNum, header, numCols, numRows, width, height);
    }
  }
  else if (header.m_type==C_CustomShape && input->tell()+12<=header.m_dataOffset)
  {
    ShapeType shapeType = getShapeType((unsigned char)readU16(input));
    if (shapeType != UNKNOWN_SHAPE)
      m_collector->setShapeType(seqNum, shapeType);
    auto flags=readU16(input);
    if (flags&3)
      m_collector->setShapeFlip(seqNum, flags&2, flags&1);
    int rot=(flags>>2)&3;
    if (rot)
      m_collector->setShapeRotation(seqNum,360-90*rot);
    /*
      for (int i=0; i<4; ++i)
      m_collector->setAdjustValue(seqNum, i, readS16(input));
    */
  }
  else if (header.m_type==C_Line && input->tell()+18<=header.m_dataOffset)
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
      m_collector->addShapeLine(seqNum, Line(ColorReference(translate2kColorReference(bColors[wh])), unsigned(widths[wh]*12700), widths[wh]>0));
    }
  }
  else if (widths[0]>0)
  {
    m_collector->addShapeLine(seqNum, Line(ColorReference(translate2kColorReference(bColors[0])), unsigned(widths[0]*12700), true));
    m_collector->setShapeBorderImageId(seqNum, unsigned(borderId));
    m_collector->setShapeBorderPosition(seqNum, OUTSIDE_SHAPE);
  }
  if (patternId&0x80)
  {
    struct GradientData
    {
      GradientData(GradientFill::Style style, double angle, boost::optional<double> cx=boost::none, boost::optional<double> cy=boost::none, bool swapColor=false)
        : m_style(style)
        , m_angle(angle)
        , m_cx(cx)
        , m_cy(cy)
        , m_swapColor(swapColor)
      {
      }
      GradientFill::Style m_style;
      double m_angle;
      boost::optional<double> m_cx;
      boost::optional<double> m_cy;
      bool m_swapColor;
    };
    static GradientData const(gradients[])=
    {
      {GradientFill::G_Rectangular,0, 0.5,0.5},
      {GradientFill::G_Rectangular,0, 0,0},
      {GradientFill::G_Rectangular,0, 1,0},
      {GradientFill::G_Rectangular,0, 1,1},
      {GradientFill::G_Rectangular,0, 0,1},
      {GradientFill::G_Ellipsoid,0, 0.5,0.5},
      {GradientFill::G_Ellipsoid,0, 0,0},
      {GradientFill::G_Ellipsoid,0, 1,0},
      {GradientFill::G_Ellipsoid,0, 0,1},
      {GradientFill::G_Ellipsoid,0, 0,1},
      {GradientFill::G_Linear,0, boost::none,boost::none, true},
      {GradientFill::G_Linear,0},
      {GradientFill::G_Linear,90},
      {GradientFill::G_Linear,90, boost::none,boost::none, true},
      {GradientFill::G_Axial,0, boost::none,boost::none, true},
      {GradientFill::G_Axial,90, boost::none,boost::none, true},
      {GradientFill::G_Axial,45},
      {GradientFill::G_Axial,-45},
      {GradientFill::G_Linear,45, boost::none,boost::none, true},
      {GradientFill::G_Linear,-45, boost::none,boost::none, true},
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // triangle
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // triangle
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // star
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // star
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // star
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // star
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // star
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // almost ellipsoid
      {GradientFill::G_Rectangular,0, 0.5,0.5},
      {GradientFill::G_Rectangular,0, 0.5,0.5},
      {GradientFill::G_Rectangular,0, 0.5,0.5},
      {GradientFill::G_Rectangular,0, 0.5,0.5},
      {GradientFill::G_Rectangular,0, 0.5,0.5},
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // pentagon
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // pentagon
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // concave hexa
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // hexa
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // up arrow
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // heart
      {GradientFill::G_Ellipsoid,0, 0.5,0.5},
      {GradientFill::G_Ellipsoid,0, 0.5,0.5},
      {GradientFill::G_Ellipsoid,0, 0.5,0.5},
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // spiral
      {GradientFill::G_Ellipsoid,0, 0.5,0.5}, // spiral 2
    };
    patternId&=0x7f;
    if (unsigned(patternId)<MSPUB_N_ELEMENTS(gradients))
    {
      auto const &data=gradients[patternId];
      auto gradient=std::make_shared<GradientFill>(m_collector, data.m_style, data.m_angle, data.m_cx, data.m_cy);
      gradient->addColor(ColorReference(translate2kColorReference(colors[data.m_swapColor ? 1 : 0])), 0, 1);
      gradient->addColor(ColorReference(translate2kColorReference(colors[data.m_swapColor ? 0 : 1])), 1, 1);
      m_collector->setShapeFill(seqNum, gradient, false);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseShapeFormat: unknown gradiant =%d\n", patternId));
    }
  }
  else if (patternId)
  {
    if (patternId==1 || patternId==2)
      m_collector->setShapeFill(seqNum, std::make_shared<SolidFill>(ColorReference(translate2kColorReference(colors[2-patternId])), 1, m_collector), false);
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
                                (m_collector,(uint8_t const(&)[8])(patterns[8*(patternId-3)]),
                                 ColorReference(translate2kColorReference(colors[1])),
                                 ColorReference(translate2kColorReference(colors[0]))), false);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser97::parseShapeFormat: unknown pattern =%d\n", patternId));
    }
  }
}
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
