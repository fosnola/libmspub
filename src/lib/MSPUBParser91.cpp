/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "MSPUBParser91.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <memory>

#include "MSPUBCollector.h"
#include "MSPUBTypes.h"
#include "libmspub_utils.h"

namespace libmspub
{
struct BlockInfo91
{
  BlockInfo91()
    : m_id(0)
    , m_parentId(0)
    , m_offset(0)
    , m_data(0)
    , m_flags(0)
    , m_child()
  {
  }
  //! the main id
  int m_id;
  //! the parent id: -1 means none
  int m_parentId;
  //! the file offset
  unsigned m_offset;
  //! a id or ...
  int m_data;
  //! the remaining flags
  int m_flags;
  //! the children
  std::vector<BlockInfo91> m_child;
};

struct TextPLCHeader91
{
  explicit TextPLCHeader91()
    : m_dataOffset(0)
    , m_textOffset(0)
    , m_N(0)
    , m_maxN(0)
    , m_dataSize(0)
    , m_positions()
  {
  }
  //! the data begin offset
  unsigned m_dataOffset;
  //! the text position
  unsigned m_textOffset;
  //! the number of data
  int m_N;
  //! the maximum data
  int m_maxN;
  //! the header size
  int m_dataSize;
  //! the data position
  std::vector<unsigned> m_positions;
};

struct ZoneHeader91
{
  explicit ZoneHeader91()
    : m_dataOffset(0)
    , m_N(0)
    , m_maxN(0)
    , m_type(-1)
    , m_headerSize(0)
    , m_lastValue(0)
    , m_positions()
    , m_values()
  {
  }
  //! the data begin offset
  unsigned m_dataOffset;
  //! the number of data
  int m_N;
  //! the maximum data
  int m_maxN;
  //! the type
  int m_type;
  //! the header size
  int m_headerSize;
  //! the last position
  unsigned m_lastValue;
  //! the data position
  std::vector<unsigned> m_positions;
  //! extra header values
  std::vector<int> m_values;
};

struct MSPubParser91Data
{
  MSPubParser91Data()
    : m_pagesId()
    , m_idToBlockMap()
  {
    for (auto &id : m_mainZoneIds) id=-1;
  }

  //! the main zone id: 0:unknown, 1:background, 2:unknown, 3:layout
  int m_mainZoneIds[4];
  //! the page zone id
  std::vector<unsigned> m_pagesId;
  //! the map id tof main block
  std::map<unsigned,BlockInfo91> m_idToBlockMap;
};

MSPUBParser91::MSPUBParser91(librevenge::RVNGInputStream *input, MSPUBCollector *collector)
  : MSPUBParser(input, collector)
  , m_data(new MSPubParser91Data)
{
  m_collector->useEncodingHeuristic();
}

void MSPUBParser91::translateCoordinateIfNecessary(int &x, int &y) const
{
  const int offset = 14904; // unsure or 14940
  if (std::numeric_limits<int>::min() + offset > x)
    x=std::numeric_limits<int>::min();
  else
    x-=offset;
  if (std::numeric_limits<int>::min() + offset > y)
    y=std::numeric_limits<int>::min();
  else
    y-=offset;
}

ColorReference MSPUBParser91::getColor(int colorId) const
{
  if (colorId<0 || colorId>7)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::getColor: unknown color id=%d\n", colorId));
    return ColorReference(0);
  }
  // 0: black, 1: white, 2: red, 3: green, blue, yellow, cyan magenta in bgr
  unsigned const colors[]= {0x000000, 0xffffff, 0x0000ff, 0x00ff00, 0xff0000, 0x00ffff, 0xffff00, 0xc000c0};
  return ColorReference(colors[colorId]);
}

bool MSPUBParser91::parse()
{
  if (!parseContents(m_input))
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parse: Couldn't parse contents stream.\n"));
    return false;
  }
  return m_collector->go();
}

bool MSPUBParser91::parseContents(librevenge::RVNGInputStream *input)
{
  input->seek(0xc, librevenge::RVNG_SEEK_SET);
  unsigned offsets[8]; // Text, Document, unknown, pages, list of block, font, border arts, unknown
  for (auto &offset : offsets) offset=readU32(input);
  if (!offsets[1] || !offsets[3] || !offsets[4] || !offsets[5])
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseContents: can not find main zone.\n"));
    return false;
  }
  if (offsets[0] && input->seek(offsets[0], librevenge::RVNG_SEEK_SET)==0)
    parseContentsTextIfNecessary(input);
  if (input->seek(offsets[1], librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseContents: can not find the document offset.\n"));
    return false;
  }
  parseDocument(input);
  if (input->seek(offsets[3], librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseContents: can not find the page offset.\n"));
    return false;
  }
  parsePageIds(input);
  if (input->seek(offsets[5], librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseContents: can not find the font offset.\n"));
    return false;
  }
  parseFonts(input);
  if (offsets[6] && input->seek(offsets[6], librevenge::RVNG_SEEK_SET)==0)
    parseBorderArts(input);

  if (input->seek(offsets[4], librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseContents: can not find the trailer offset.\n"));
    return false;
  }
  if (!parseBlockInfos(input))
    return false;

  auto backIt=m_data->m_idToBlockMap.find(m_data->m_mainZoneIds[1]);
  if (backIt!=m_data->m_idToBlockMap.end() && !backIt->second.m_child.empty())
  {
    unsigned masterId=unsigned(m_data->m_mainZoneIds[1]);
    auto pageIt=m_data->m_idToBlockMap.find(unsigned(masterId));
    if (pageIt==m_data->m_idToBlockMap.end())
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseContents: can not find the shape for page=%d.\n", masterId));
    }
    else
    {
      m_collector->addPage(masterId);
      m_collector->designateMasterPage(masterId);
      for (auto const &id : m_data->m_pagesId)
        m_collector->setMasterPage(unsigned(id), masterId);

      m_collector->setShapePage(masterId,masterId);
      m_collector->beginGroup();
      m_collector->setCurrentGroupSeqNum(masterId);
      parseShapesList(input, pageIt->second);
      m_collector->endGroup();
    }
  }

  for (auto const &id : m_data->m_pagesId)
  {
    auto pageIt=m_data->m_idToBlockMap.find(unsigned(id));
    if (pageIt==m_data->m_idToBlockMap.end())
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseContents: can not find the shape for page=%d.\n", id));
      continue;
    }
    m_collector->setShapePage(id,id);
    m_collector->beginGroup();
    m_collector->setCurrentGroupSeqNum(id);
    parseShapesList(input, pageIt->second);
    m_collector->endGroup();
  }
  return true;
}

void MSPUBParser91::parseContentsTextIfNecessary(librevenge::RVNGInputStream *input)
{
  // set the default parameter
  CharacterStyle defaultCharStyle;
  defaultCharStyle.textSizeInPt=10;
  m_collector->addDefaultCharacterStyle(defaultCharStyle);
  for (int i=0; i<8; ++i) m_collector->addTextColor(getColor(i));

  input->seek(14, librevenge::RVNG_SEEK_CUR);
  unsigned textStart = readU32(input);
  unsigned textEnd = readU32(input);
  unsigned index[3];
  for (auto &i : index) i=readU16(input);
  unsigned plcs[5];
  for (auto &p : plcs) p = readU32(input);

  std::map<unsigned, CharacterStyle> posToSpanMap;
  for (unsigned id=index[0]; id<index[1]; ++id)
    parseSpanStyles(input, id, posToSpanMap);
  std::map<unsigned, ParagraphStyle> posToParaMap;
  for (unsigned id=index[1]; id<index[2]; ++id)
    parseParagraphStyles(input, id, posToParaMap);

  std::vector<unsigned> textLimits;
  if (plcs[2] && input->seek(plcs[2], librevenge::RVNG_SEEK_SET)==0)
  {
    TextPLCHeader91 plcHeader;
    parseTextPLCHeader(input, plcHeader);
    textLimits=plcHeader.m_positions;
  }
  size_t N=textLimits.size();
  textLimits.insert(textLimits.begin(),0);
  if (plcs[3] && input->seek(plcs[3], librevenge::RVNG_SEEK_SET)==0)
  {
    TextPLCHeader91 plcHeader;
    parseTextPLCHeader(input, plcHeader);
    if (plcHeader.m_positions.size()!=2*N || plcHeader.m_dataSize!=10)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseContentsTextIfNecessary: oops, unexpected plc3 zone.\n"));
      N=0;
    }
    for (size_t i=0; i<N; ++i)
    {
      input->seek(plcHeader.m_dataOffset+20*i+4, librevenge::RVNG_SEEK_SET);
      unsigned zId=readU16(input);
      if (textLimits[i+1]<textLimits[i] || textLimits[i+1]>textEnd)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser91::parseContentsTextIfNecessary: text zone %d, bad limit.\n", int(i)));
        continue;
      }
      std::vector<unsigned char> spanChars;
      spanChars.reserve(textLimits[i+1]-textLimits[i]);
      input->seek(textStart+textLimits[i], librevenge::RVNG_SEEK_SET);
      std::vector<TextParagraph> shapeParas;
      std::vector<TextSpan> paraSpans;
      CharacterStyle charStyle;
      ParagraphStyle paraStyle;
      // sometimes the paragraph is defined in pos-1, so let check
      auto pIt=posToParaMap.lower_bound(input->tell());
      if (pIt!=posToParaMap.begin())
      {
        --pIt;
        if (pIt->first>=input->tell()-2) paraStyle=pIt->second;
      }
      for (unsigned p=textLimits[i]; p<textLimits[i+1]; ++p)
      {
        auto cIt=posToSpanMap.find(input->tell());
        if (cIt!=posToSpanMap.end())
        {
          if (!spanChars.empty())
          {
            paraSpans.push_back(TextSpan(spanChars,charStyle));
            spanChars.clear();
          }
          charStyle=cIt->second;
        }
        pIt=posToParaMap.find(input->tell());
        if (pIt!=posToParaMap.end())
        {
          if (!spanChars.empty())
          {
            paraSpans.push_back(TextSpan(spanChars,charStyle));
            spanChars.clear();
          }
          if (!paraSpans.empty())
          {
            shapeParas.push_back(TextParagraph(paraSpans, paraStyle));
            paraSpans.clear();
          }
          paraStyle=pIt->second;
        }
        unsigned char ch=readU8(input);
        if (ch == 0xB) // Pub97 interprets vertical tab as nonbreaking space.
        {
          spanChars.push_back('\n');
        }
        else if (ch == 0x0D || ch==0x0A)
        {
          // ignore the 0x0D and advance past the 0x0A
        }
        else if (ch == 0x0C)
        {
          // ignore the 0x0C
        }
        else if (ch=='#' && p+1<textLimits[i+1])
        {
          // look for page field, we do not want to change the style
          auto actPos=input->tell();
          auto nextCh=readU8(input);
          if (nextCh==0x5)
          {
            ++p;
            if (!spanChars.empty())
            {
              paraSpans.push_back(TextSpan(spanChars,charStyle));
              spanChars.clear();
            }
            TextSpan pageSpan(spanChars,charStyle);
            pageSpan.isPageField=true;
            paraSpans.push_back(pageSpan);
          }
          else
          {
            input->seek(actPos, librevenge::RVNG_SEEK_SET);
            spanChars.push_back(ch);
          }
        }
        else if (ch==0x5 || ch==0x9 || ch>0x1f)
        {
          spanChars.push_back(ch);
        }
        else
        {
          MSPUB_DEBUG_MSG(("MSPUBParser91::parseContentsTextIfNecessary:find odd character %x\n", unsigned(ch)));
        }
      }
      if (!spanChars.empty())
        paraSpans.push_back(TextSpan(spanChars,charStyle));
      if (paraStyle.m_align)  std::cout << "align=" << *paraStyle.m_align << "\n";
      if (!paraSpans.empty())
        shapeParas.push_back(TextParagraph(paraSpans, paraStyle));
      m_collector->addTextString(shapeParas, zId);
    }
  }
}

bool MSPUBParser91::parseParagraphStyles(librevenge::RVNGInputStream *input, unsigned index, std::map<unsigned, ParagraphStyle> &posToStyle)
{
  if (input->seek((index+1)*0x200-1, librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseParagraphStyles: can not use index=%x\n", index));
    return false;
  }
  unsigned N=unsigned(readU8(input));
  if ((N+1)*5>0x200)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseParagraphStyles: N value=%d is too big for index=%x\n", N, index));
    return false;
  }
  input->seek(index*0x200, librevenge::RVNG_SEEK_SET);
  std::vector<unsigned> positions;
  positions.resize(N+1);
  for (auto &p : positions) p=readU32(input);
  std::vector<uint8_t> stylePos;
  stylePos.reserve(N);
  for (unsigned i=0; i<N; ++i) stylePos.push_back(readU8(input));
  for (unsigned i=0; i<N; ++i)
  {
    auto const &offs=stylePos[i];
    if (offs==0)
    {
      posToStyle[positions[i]]=ParagraphStyle();
      continue;
    }
    input->seek(index*0x200+2*offs, librevenge::RVNG_SEEK_SET); // skip small size
    uint8_t len=readU8(input);
    uint8_t tabPos=readU8(input);
    if (tabPos<2 || 2*offs+1+tabPos>0x200 || 2*len+1<tabPos)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseParagraphStyles: can not read len for i=%x for index=%x\n", i, index));
      posToStyle[positions[i]]=ParagraphStyle();
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
        MSPUB_DEBUG_MSG(("MSPUBParser91::parseParagraphStyles: unknown align=%d\n", align));
        break;
      }
    }
    if (tabPos>=5)
      style.m_firstLineIndentEmu=readS16(input)*635;
    if (tabPos>=7)
      style.m_leftIndentEmu=readU16(input)*635;
    if (tabPos>=9)
      style.m_rightIndentEmu=readU16(input)*635;
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
        MSPUB_DEBUG_MSG(("MSPUBParser91::parseParagraphStyles: can not read tab len for i=%x for index=%x\n", i, index));
      }
      else
      {
        input->seek(1, librevenge::RVNG_SEEK_CUR);
        auto nTabs=readU8(input);
        if (3*nTabs+2>tabLen)
        {
          MSPUB_DEBUG_MSG(("MSPUBParser91::parseParagraphStyles: bad tabs numbers=%d for index=%x\n", int(nTabs), index));
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
              MSPUB_DEBUG_MSG(("MSPUBParser91::parseParagraphStyles: find unexpected flags=%x\n", fl));
            }
            style.m_tabStops.push_back(tab);
          }
        }
      }
    }
    posToStyle[positions[i]]=style;
  }
  return true;
}

bool MSPUBParser91::parseSpanStyles(librevenge::RVNGInputStream *input, unsigned index, std::map<unsigned, CharacterStyle> &posToStyle)
{
  if (input->seek((index+1)*0x200-1, librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseSpanStyles: can not use index=%x\n", index));
    return false;
  }
  unsigned N=unsigned(readU8(input));
  if ((N+1)*5>0x200)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseSpanStyles: N value=%d is too big for index=%x\n", N, index));
    return false;
  }
  input->seek(index*0x200, librevenge::RVNG_SEEK_SET);
  std::vector<unsigned> positions;
  positions.resize(N+1);
  for (auto &p : positions) p=readU32(input);
  std::vector<uint8_t> stylePos;
  stylePos.reserve(N);
  for (unsigned i=0; i<N; ++i) stylePos.push_back(readU8(input));

  for (unsigned i=0; i<N; ++i)
  {
    auto const &offs=stylePos[i];
    if (offs==0)
    {
      posToStyle[positions[i]]=CharacterStyle();
      continue;
    }
    input->seek(index*0x200+2*offs, librevenge::RVNG_SEEK_SET);
    uint8_t len=readU8(input);
    if (len==0 || 2*offs+1+len>0x200)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseSpanStyles: can not read len for i=%x for index=%x\n", i, index));
      posToStyle[positions[i]]=CharacterStyle();
      continue;
    }
    CharacterStyle style;
    int textSizeVariationFromDefault = 0;
    auto begin=input->tell();
    unsigned char biFlags = readU8(input);
    style.bold = biFlags & 0x1;
    style.italic = biFlags & 0x2;
    style.smallCaps = biFlags & 0x4;
    style.allCaps = biFlags & 0x8;
    if (len >= 3)
    {
      input->seek(begin + 0x2, librevenge::RVNG_SEEK_SET);
      style.fontIndex = readU8(input);
    }
    if (len >= 5)
    {
      input->seek(begin + 0x4, librevenge::RVNG_SEEK_SET);
      textSizeVariationFromDefault =
        len >= 6 ? readS16(input) : readS8(input);
    }
    if (len >= 7)
    {
      int baseLine=readS8(input);
      style.superSubType = baseLine<0 ? SUBSCRIPT : baseLine>0 ? SUPERSCRIPT : NO_SUPER_SUB;
    }
    if (len >= 8)
      style.colorIndex=readU8(input);
    if (len >= 9)
    {
      unsigned char fl1=readU8(input), fl2=0;
      if (fl1&0x7f)
      {
        unsigned stretch=fl1&0x7f;
        style.letterSpacingInPt=double(stretch)/4;
        if (stretch>88) // check limit
          style.letterSpacingInPt=*style.letterSpacingInPt-double(0x80)/4;
      }
      if (len>=10) fl2=readU8(input);
      if ((fl1&0x80) && (fl2&1))
        style.underline = Underline::Double;
      else if ((fl1&0x80) || (fl2&1)) // fl1&0x80: underline all, fl2&1: underline word
        style.underline = Underline::Single;
    }
    style.textSizeInPt = 10 +
                         static_cast<double>(textSizeVariationFromDefault) / 2;
    posToStyle[positions[i]]=style;
  }
  return true;
}

bool MSPUBParser91::parseBlockInfos(librevenge::RVNGInputStream *input)
{
  unsigned numBlocks = readU16(input);
  input->seek(2, librevenge::RVNG_SEEK_CUR); // skip max bloc id
  auto &mainIdToBlockMap=m_data->m_idToBlockMap;
  std::vector<BlockInfo91> otherBlocks;
  for (unsigned i = 0; i < numBlocks; ++i)
  {
    BlockInfo91 block;
    block.m_id=int(readU16(input));
    block.m_parentId=int(readS16(input));
    block.m_offset = readU16(input);
    block.m_data=int(readS16(input));
    block.m_flags=int(readU16(input));
    if (((block.m_flags>>8)&0x8f)==0x81)
    {
      if (mainIdToBlockMap.find(block.m_id)!=mainIdToBlockMap.end())
      {
        MSPUB_DEBUG_MSG(("MSPUBParser91::parseBlockInfos: a block with id=%d already exists\n", block.m_id));
      }
      else
        mainIdToBlockMap[block.m_id]=block;
    }
    else
      otherBlocks.push_back(block);
  }
  for (auto bl : otherBlocks)
  {
    auto pIt=mainIdToBlockMap.find(bl.m_parentId);
    if (pIt==mainIdToBlockMap.end())
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseBlockInfos: can not find parent %d for block with id=%d\n", bl.m_parentId, bl.m_id));
      continue;
    }
    bl.m_offset += pIt->second.m_offset;
    pIt->second.m_child.push_back(bl);
  }
#ifdef DEBUG
  for (auto const &it : mainIdToBlockMap)
  {
    auto const &block=it.second;
    std::cout << "S" << it.first << "[" << std::hex << block.m_flags << std::dec << "]:";
    if (block.m_data) std::cout << "data=" << block.m_data << ",";
    if (!block.m_child.empty())
    {
      std::cout << "childs=[";
      for (auto const &ch : block.m_child)
      {
        std::cout << "S" << ch.m_id;
        if (ch.m_flags!=0x8500) std::cout << "[" << std::hex << ch.m_flags << std::dec << "]";
        if (ch.m_data!=-1) std::cout << ":dt=" << ch.m_data;
        std::cout << ",";
      }
      std::cout << "],";
    }
    std::cout << "\n";
  }
#endif
  return true;
}

bool MSPUBParser91::parseShapesList(librevenge::RVNGInputStream *input, BlockInfo91 const &info)
{
  if (((info.m_flags>>8)&0x85)!=0x81)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseShapesList: block %d flag[%x] seems bad\n", info.m_id, info.m_flags));
    return false;
  }
  // 0:N, 2:N[max], 4:id, 6:unkn, 8-15: the margins (or -1,0,0,0), 16-19: row,col, 20: unkn, 22: lastpos then list of child pos
  for (auto const &bl : info.m_child)
  {
    m_collector->setShapePage(unsigned(bl.m_id), unsigned(info.m_id));
    parseShape(input, bl);
  }
  return true;
}

bool MSPUBParser91::parseShape(librevenge::RVNGInputStream *input, BlockInfo91 const &info)
{
  if (((info.m_flags>>8)&0x85)!=0x85)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseShape: block %d flag[%x] seems bad\n", info.m_id, info.m_flags));
    return false;
  }
  m_collector->setShapeOrder(info.m_id);
  input->seek(info.m_offset,librevenge::RVNG_SEEK_SET);
  unsigned type=readU16(input);
  input->seek(2, librevenge::RVNG_SEEK_CUR); // block id
  int dims[4];
  for (auto &d : dims) d=int(readU16(input));
  translateCoordinateIfNecessary(dims[0],dims[1]);
  translateCoordinateIfNecessary(dims[2],dims[3]);
  m_collector->setShapeCoordinatesInEmu(info.m_id,dims[0]*635,dims[1]*635,dims[2]*635,dims[3]*635);
  input->seek(8, librevenge::RVNG_SEEK_CUR); // another box
  int colors[3];
  for (int i=0; i<2; ++i) colors[i]=int(readU8(input));
  int patternId=int(readU8(input));
  colors[2]=int(readU8(input));
  auto val=readU8(input);
  double width=(val&0x80) ? double(val&0x7f)/4 : double(val);
  int borderId=int(readS16(input));
  input->seek(3, librevenge::RVNG_SEEK_CUR); // 0: sometimes line width, 2: ?
  auto flags=readU16(input);
  switch (type)
  {
  case 0: // text
    m_collector->setShapeType(info.m_id, RECTANGLE);
    m_collector->addTextShape(info.m_id, info.m_id);
    break;
  case 2:
  case 3:
  {
    // now 0-6: unknown, 7-14: dim1, 15-22: dim2, 23-24: unknown, 25-28: size
    auto dataIt=m_data->m_idToBlockMap.find(info.m_data);
    if (dataIt==m_data->m_idToBlockMap.end() || info.m_data<0)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseShape: can not data block %d for block with id=%d\n", info.m_data, info.m_id));
      break;
    }
    m_collector->setShapeType(info.m_id,PICTURE_FRAME);
    m_collector->setShapeImgIndex(info.m_id,info.m_data);
    parseImage(input, dataIt->second);
    break;
  }
  case 4:
    if ((flags&0x10)==0)
      m_collector->setShapeFlip(info.m_id, true, false);
    if (flags&0x20)
      m_collector->setShapeEndArrow(info.m_id, Arrow(TRIANGLE_ARROW, MEDIUM, MEDIUM));
    if (flags&0x40)
      m_collector->setShapeBeginArrow(info.m_id, Arrow(TRIANGLE_ARROW, MEDIUM, MEDIUM));
    m_collector->setShapeType(info.m_id,LINE);
    break;
  case 5:
    m_collector->setShapeType(info.m_id,RECTANGLE);
    break;
  case 6:
    m_collector->setShapeType(info.m_id,ROUND_RECTANGLE);
    break;
  case 7:
    m_collector->setShapeType(info.m_id,ELLIPSE);
    break;
  default:
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseShape: find unexpected type=%d\n", type));
    break;
  }
  if (width>0)
  {
    m_collector->addShapeLine(info.m_id, Line(getColor(colors[2]), width*12700, true));
    if (borderId>=0 && (flags&4))
    {
      m_collector->setShapeBorderImageId(info.m_id, unsigned(borderId));
      m_collector->setShapeBorderPosition(info.m_id, OUTSIDE_SHAPE);
    }
  }
  if (patternId)
  {
    if (patternId==1 || patternId==2)
      m_collector->setShapeFill(info.m_id, std::make_shared<SolidFill>(getColor(colors[2-patternId]), 1, m_collector), false);
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

      m_collector->setShapeFill(info.m_id, std::make_shared<Pattern88Fill>
                                (m_collector,(uint8_t const(&)[8])(patterns[8*(patternId-3)]),getColor(colors[1]), getColor(colors[0])), false);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseShape: unknown pattern =%d\n", patternId));
    }
  }
  return true;
}

bool MSPUBParser91::parseImage(librevenge::RVNGInputStream *input, BlockInfo91 const &info)
{
  if (((info.m_flags>>8)&0x81)!=0x81)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseImage: block %d flag[%x] seems bad\n", info.m_id, info.m_flags));
    return false;
  }
  if (input->seek(info.m_offset-4,librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseImage: can not find the block %d zone\n", info.m_id));
    return false;
  }
  unsigned long len=readU32(input);
  if (input->seek(info.m_offset+len,librevenge::RVNG_SEEK_SET)!=0 || len<10)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseImage: can not find the block %d end\n", info.m_id));
    return false;
  }
  input->seek(info.m_offset, librevenge::RVNG_SEEK_SET);
  unsigned header[2];
  for (auto &val : header) val=readU16(input);
  ImgType imgType=UNKNOWN;
  // find some embedded ole and some wmf picture
  librevenge::RVNGBinaryData img;
  if (header[0]>=1 && header[0]<=2 && header[1]>=9 && header[1]<=0xa)
    imgType=WMF;
  else if (header[0]==0x501 && header[1]==0)
  {
    input->seek(info.m_offset, librevenge::RVNG_SEEK_SET);
    if (parseOLEPicture(input, len, imgType, img))
    {
      m_collector->addImage(info.m_id, imgType, img);
      return true;
    }
  }
  input->seek(info.m_offset, librevenge::RVNG_SEEK_SET);
  while (len > 0 && stillReading(input, (unsigned long)-1))
  {
    unsigned long howManyRead = 0;
    const unsigned char *buf = input->read(len, howManyRead);
    img.append(buf, howManyRead);
    len -= howManyRead;
  }
  m_collector->addImage(info.m_id, imgType, img);
  return true;
}

bool MSPUBParser91::parseMetafilePicture(librevenge::RVNGInputStream *input, unsigned length, ImgType &type, librevenge::RVNGBinaryData &img)
{
  if (length<12)
    return false;
  // 0-1: type, 2-5: dim, 6-7: handle
  input->seek(8, librevenge::RVNG_SEEK_CUR); // seek handle
  unsigned header[2];
  for (auto &val : header) val=readU16(input);
  if (header[0]<1 && header[0]>2 && header[1]<9 && header[1]>0xa)
    return false;
  type=WMF;
  input->seek(-4, librevenge::RVNG_SEEK_CUR);
  length -= 8;
  while (length > 0 && stillReading(input, (unsigned long)-1))
  {
    unsigned long howManyRead = 0;
    const unsigned char *buf = input->read(length, howManyRead);
    img.append(buf, howManyRead);
    length -= howManyRead;
  }
  return true;
}

bool MSPUBParser91::parseOLEPicture(librevenge::RVNGInputStream *input, unsigned length, ImgType &type, librevenge::RVNGBinaryData &img)
{
  if (length<24+4) return false; // to small
  long endPos=input->tell()+length;
  if (readU32(input)!=0x501) return false;
  auto fType=readU32(input);
  if (fType!=2)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseOLEPicture: reading type=%d picture is not implemented\n", int(fType)));
    return false;
  }
  std::string names[3];
  for (auto &name : names)
  {
    auto sSz=readU32(input);
    if (sSz==0) continue;
    if (endPos-input->tell()<sSz+4)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseOLEPicture: can not read a name\n"));
      return false;
    }
    for (unsigned i=0; i<sSz; ++i)
    {
      auto c=readU8(input);
      if (c)
        name+=c;
      else if (i+1!=sSz)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser91::parseOLEPicture: can not read a name\n"));
        return false;
      }
    }
  }
  auto dSz=readU32(input);
  long actPos=input->tell();
  if (dSz>0x40000000 || dSz<10 || dSz>(unsigned)(endPos-actPos))
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseOLEPicture: pict size seems bad\n"));
    return false;
  }
  if (names[0]=="METAFILEPICT")
    return parseMetafilePicture(input, dSz, type, img);
  // ok save the data and hope that it can be read by somebody
  while (dSz > 0 && stillReading(input, (unsigned long)-1))
  {
    unsigned long howManyRead = 0;
    const unsigned char *buf = input->read(dSz, howManyRead);
    img.append(buf, howManyRead);
    dSz -= howManyRead;
  }
  return true;
}

bool MSPUBParser91::parseBorderArts(librevenge::RVNGInputStream *input)
{
  ZoneHeader91 header;
  if (!parseZoneHeader(input, header, false)) return false;
  std::set<unsigned> listPos(header.m_positions.begin(), header.m_positions.end());
  listPos.insert(header.m_dataOffset+header.m_lastValue);
  for (size_t i=0; i<header.m_positions.size(); ++i)
  {
    // position=0xffff means none, so must check this
    if (header.m_positions[i]>header.m_dataOffset+header.m_lastValue) continue;

    auto it=listPos.find(header.m_positions[i]);
    if (it==listPos.end() || ++it==listPos.end())
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseBorderArts: can not find the end position for art=%d\n",int(i)));
      continue;
    }
    auto endPos=*it;
    if (endPos<66+header.m_positions[i]+4)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseBorderArts: art zone %d seems to short\n",int(i)));
      continue;
    }
    input->seek(header.m_positions[i]+50, librevenge::RVNG_SEEK_SET);
    unsigned decal[8];
    for (auto &d : decal) d = readU16(input);
    std::map<unsigned, unsigned> offsetToImage;
    for (int off=0; off<8; ++off)
    {
      auto oIt=offsetToImage.find(decal[off]);
      if (oIt!=offsetToImage.end())
      {
        m_collector->setBorderImageOffset(i,oIt->second);
        std::cout << "ADD " << oIt->second << "\n";
        continue;
      }
      input->seek(header.m_positions[i]+decal[off], librevenge::RVNG_SEEK_SET);
      // check that we have a picture
      unsigned headerVals[2];
      for (auto &val : headerVals) val=readU16(input);
      if (headerVals[0]<1 || headerVals[0]>2 || headerVals[1]<9 || headerVals[1]>10)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser91::parseBorderArts: can not find the wmf picture for art zone %d\n",int(i)));
        continue;
      }
      input->seek(2, librevenge::RVNG_SEEK_CUR);
      unsigned pictSize=readU32(input);
      if (pictSize<9 || header.m_positions[i]+decal[off]+2*pictSize>endPos)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser91::parseBorderArts: art zone %d pictSize seems bad\n",int(i)));
        continue;
      }
      // ok, let save the picure
      pictSize*=2;
      input->seek(header.m_positions[i]+decal[off], librevenge::RVNG_SEEK_SET);
      librevenge::RVNGBinaryData &img = *(m_collector->addBorderImage(WMF, i));
      while (pictSize > 0 && stillReading(input, (unsigned long)-1))
      {
        unsigned long howManyRead = 0;
        const unsigned char *buf = input->read(pictSize, howManyRead);
        img.append(buf, howManyRead);
        pictSize -= howManyRead;
      }
      unsigned newId=offsetToImage.size();
      m_collector->setBorderImageOffset(i,newId);
      offsetToImage[decal[off]]=newId;
      std::cout << "ADD " << newId << "\n";
    }
  }
  return true;
}

bool MSPUBParser91::parseDocument(librevenge::RVNGInputStream *input)
{
  input->seek(2, librevenge::RVNG_SEEK_CUR);
  for (auto &id : m_data->m_mainZoneIds) id=int(readU16(input));
  input->seek(10, librevenge::RVNG_SEEK_CUR); // 230,0,1,0, page type?
  unsigned width = readU16(input);
  unsigned height = readU16(input);
  m_collector->setWidthInEmu(width*635);
  m_collector->setHeightInEmu(height*635);
  return true;
}

bool MSPUBParser91::parseFonts(librevenge::RVNGInputStream *input)
{
  ZoneHeader91 header;
  if (!parseZoneHeader(input, header, false)) return false;
  for (auto ptr : header.m_positions)
  {
    // skip font id
    if (input->seek(ptr+2, librevenge::RVNG_SEEK_SET)!=0)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser91::parseFonts: unexpected fonts positions\n"));
      continue;
    }
    std::vector<unsigned char> name;
    while (!input->isEnd())
    {
      unsigned char c=readU8(input);
      if (!c) break;
      name.push_back(c);
    }
    m_collector->addFont(name);
  }
  return true;
}

bool MSPUBParser91::parsePageIds(librevenge::RVNGInputStream *input)
{
  ZoneHeader91 header;
  if (!parseZoneHeader(input, header, true)) return false;
  m_data->m_pagesId.resize(size_t(header.m_N));
  for (auto &id : m_data->m_pagesId)
  {
    id=readU16(input);
    m_collector->addPage(id);
  }
  return true;
}

bool MSPUBParser91::parseZoneHeader(librevenge::RVNGInputStream *input, ZoneHeader91 &header, bool doNotReadPositions)
{
  unsigned pos=input->tell();
  header.m_N=int(readU16(input));
  header.m_maxN=int(readU16(input));
  header.m_lastValue=int(readU16(input));
  header.m_type=int(readU16(input)); // 0: pages, 2:font, 3:border art
  header.m_headerSize=int(readU16(input));
  auto const &dataPos=header.m_dataOffset=pos+header.m_headerSize;
  if (header.m_headerSize<10 || input->seek(dataPos+2*header.m_N,librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseZoneHeader: unexpected zone header for type=%d\n", header.m_type));
    return false;
  }
  input->seek(pos+10, librevenge::RVNG_SEEK_SET);
  for (int i=10; i+2<header.m_headerSize; i+=2)
    header.m_values.push_back(int(readS16(input)));
  input->seek(dataPos, librevenge::RVNG_SEEK_SET);
  if (doNotReadPositions || !header.m_N) return true;
  header.m_positions.resize(size_t(header.m_N-1));
  for (auto &p : header.m_positions) p=dataPos+readU16(input); // last is the last position
  return true;
}

bool MSPUBParser91::parseTextPLCHeader(librevenge::RVNGInputStream *input, TextPLCHeader91 &header)
{
  unsigned pos=input->tell();
  header.m_N=int(readU16(input));
  header.m_maxN=int(readU16(input));
  header.m_dataSize=int(readU16(input));
  input->seek(12, librevenge::RVNG_SEEK_CUR);
  header.m_textOffset=readU32(input);
  auto const &dataPos=header.m_dataOffset=pos+22+4*header.m_N;
  if (input->seek(dataPos+header.m_dataSize*header.m_N,librevenge::RVNG_SEEK_SET)!=0)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser91::parseTextPLCHeader: unexpected zone header\n"));
    return false;
  }
  input->seek(pos+22, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<header.m_N; ++i)
    header.m_positions.push_back(readU32(input));
  return true;
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
