/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "MSPUBParser2k.h"

#include <algorithm>
#include <utility>
#include <memory>

#include <librevenge-stream/librevenge-stream.h>

#include "ColorReference.h"
#include "Fill.h"
#include "Line.h"
#include "MSPUBCollector.h"
#include "MSPUBMetaData.h"
#include "OLEParser.h"
#include "ShapeType.h"
#include "libmspub_utils.h"

namespace libmspub
{

MSPUBParser2k::MSPUBParser2k(librevenge::RVNGInputStream *input, MSPUBCollector *collector)
  : MSPUBParser(input, collector)
  , m_imageDataChunkIndices()
  , m_oleDataChunkIndices()
  , m_specialPaperChunkIndex()
  , m_quillColorEntries()
  , m_fileIdToChunkId()
  , m_chunkChildIndicesById()
  , m_shapesAlreadySend()
  , m_version(5) // assume publisher 98
  , m_isBanner(false)
  , m_chunkIdToTextEndMap()
{
}

unsigned MSPUBParser2k::getColorIndexByQuillEntry(unsigned entry)
{
  unsigned translation = translate2kColorReference(entry);
  std::vector<unsigned>::const_iterator i_entry = std::find(m_quillColorEntries.begin(), m_quillColorEntries.end(), translation);
  if (i_entry == m_quillColorEntries.end())
  {
    m_quillColorEntries.push_back(translation);
    m_collector->addTextColor(ColorReference(translation));
    return unsigned(m_quillColorEntries.size() - 1);
  }
  return unsigned(i_entry - m_quillColorEntries.begin());
}

MSPUBParser2k::~MSPUBParser2k()
{
}

bool MSPUBParser2k::getChunkReference(unsigned seqNum, ContentChunkReference &chunk) const
{
  auto it=m_fileIdToChunkId.find(seqNum);
  if (it==m_fileIdToChunkId.end())
    return false;
  chunk=m_contentChunks.at(it->second);
  return true;
}

// Takes a line width specifier in Pub2k format and translates it into quarter points
unsigned short translateLineWidth(unsigned char lineWidth)
{
  return (lineWidth&0x80) ? (lineWidth&0x7f) : 4*lineWidth;
}

Color MSPUBParser2k::getColorBy2kHex(unsigned hex)
{
  switch ((hex >> 24) & 0xFF)
  {
  case 0x80:
  case 0x00:
    return getColorBy2kIndex(hex & 0xFF);
  case 0x90:
  case 0x20:
    return Color(hex & 0xFF, (hex >> 8) & 0xFF, (hex >> 16) & 0xFF);
  default:
    return Color();
  }
}

Color MSPUBParser2k::getColorBy2kIndex(unsigned char index)
{
  switch (index)
  {
  case 0x00:
    return Color(0, 0, 0);
  case 0x01:
    return Color(0xff, 0xff, 0xff);
  case 0x02:
    return Color(0xff, 0, 0);
  case 0x03:
    return Color(0, 0xff, 0);
  case 0x04:
    return Color(0, 0, 0xff);
  case 0x05:
    return Color(0xff, 0xff, 0);
  case 0x06:
    return Color(0, 0xff, 0xff);
  case 0x07:
    return Color(0xff, 0, 0xff);
  case 0x08:
    return Color(128, 128, 128);
  case 0x09:
    return Color(192, 192, 192);
  case 0x0A:
    return Color(128, 0, 0);
  case 0x0B:
    return Color(0, 128, 0);
  case 0x0C:
    return Color(0, 0, 128);
  case 0x0D:
    return Color(128, 128, 0);
  case 0x0E:
    return Color(0, 128, 128);
  case 0x0F:
    return Color(128, 0, 128);
  case 0x10:
    return Color(255, 153, 51);
  case 0x11:
    return Color(51, 0, 51);
  case 0x12:
    return Color(0, 0, 153);
  case 0x13:
    return Color(0, 153, 0);
  case 0x14:
    return Color(153, 153, 0);
  case 0x15:
    return Color(204, 102, 0);
  case 0x16:
    return Color(153, 0, 0);
  case 0x17:
    return Color(204, 153, 204);
  case 0x18:
    return Color(102, 102, 255);
  case 0x19:
    return Color(102, 255, 102);
  case 0x1A:
    return Color(255, 255, 153);
  case 0x1B:
    return Color(255, 204, 153);
  case 0x1C:
    return Color(255, 102, 102);
  case 0x1D:
    return Color(255, 153, 0);
  case 0x1E:
    return Color(0, 102, 255);
  case 0x1F:
    return Color(255, 204, 0);
  case 0x20:
    return Color(153, 0, 51);
  case 0x21:
    return Color(102, 51, 0);
  case 0x22:
    return Color(66, 66, 66);
  case 0x23:
    return Color(255, 153, 102);
  case 0x24:
    return Color(153, 51, 0);
  case 0x25:
    return Color(255, 102, 0);
  case 0x26:
    return Color(51, 51, 0);
  case 0x27:
    return Color(153, 204, 0);
  case 0x28:
    return Color(255, 255, 153);
  case 0x29:
    return Color(0, 51, 0);
  case 0x2A:
    return Color(51, 153, 102);
  case 0x2B:
    return Color(204, 255, 204);
  case 0x2C:
    return Color(0, 51, 102);
  case 0x2D:
    return Color(51, 204, 204);
  case 0x2E:
    return Color(204, 255, 255);
  case 0x2F:
    return Color(51, 102, 255);
  case 0x30:
    return Color(0, 204, 255);
  case 0x31:
    return Color(153, 204, 255);
  case 0x32:
    return Color(51, 51, 153);
  case 0x33:
    return Color(102, 102, 153);
  case 0x34:
    return Color(153, 51, 102);
  case 0x35:
    return Color(204, 153, 255);
  case 0x36:
    return Color(51, 51, 51);
  case 0x37:
    return Color(150, 150, 150);
  default:
    return Color();
  }
}

// takes a color reference in 2k format and translates it into 2k2 format that collector understands.
unsigned MSPUBParser2k::translate2kColorReference(unsigned ref2k) const
{
  if (m_version==2)
  {
    if (ref2k&0x90)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::translate2kColorReference: find unknown color flag=%x\n", ref2k));
    }
    Color c = getColorBy2kHex(ref2k&0xf);
    double delta=double((ref2k>>5)&3)/4; // pattern 0: means line color, 3: means 1/4 of color 3/4 of white
    unsigned rgb[]=
    {
      unsigned((1-delta)*double(c.r)+delta*255),
      unsigned((1-delta)*double(c.g)+delta*255),
      unsigned((1-delta)*double(c.b)+delta*255)
    };
    return unsigned((rgb[0]) | (rgb[1] << 8) | (rgb[2] << 16));
  }
  if (m_version>=3 && m_version<=4 && (ref2k>>24)==0x81)
  {
    // v3: find
    //    + 00 00 00 id
    //    + 20 RR GG BB
    //    + 81 DD 00 id with DD=80: color, DD=81-ff mixed color and white, DD=00-7f mixed color and black
    Color c = getColorBy2kHex(ref2k&0xffff);
    double delta=double((ref2k>>16)&0xff)/128-1;
    double defColor=delta*255;
    if (delta<0)
    {
      delta*=-1;
      defColor=0;
    }
    unsigned rgb[]=
    {
      unsigned((1-delta)*double(c.r)+defColor),
      unsigned((1-delta)*double(c.g)+defColor),
      unsigned((1-delta)*double(c.b)+defColor)
    };
    return unsigned((rgb[0]) | (rgb[1] << 8) | (rgb[2] << 16));
  }
  switch ((ref2k >> 24) & 0xFF)
  {
  case 0xC0: //index into user palette
  case 0xE0:
    return (ref2k & 0xFF) | (0x08 << 24);
  default:
  {
    Color c = getColorBy2kHex(ref2k);
    return unsigned((c.r) | (c.g << 8) | (c.b << 16));
  }
  }
}

ColorReference MSPUBParser2k::getColorReferenceByIndex(unsigned ref2k) const
{
  unsigned baseColor=translate2kColorReference(ref2k);
  if (m_version==5 && (ref2k&0xff000000)==0xc0000000) // changeme probably also try in v2k
  {
    unsigned mod=(ref2k>>16)&0xff;
    unsigned modifiedColor=(0x10<<24)|((mod>0x7f ? mod-0x7f : mod)<<17)|unsigned((mod>0x70? 2 : 1)<<8);
    return ColorReference(baseColor,modifiedColor);
  }
  return ColorReference(baseColor);
}

//FIXME: Valek found different values; what does this depend on?
ShapeType MSPUBParser2k::getShapeType(unsigned char shapeSpecifier)
{
  switch (shapeSpecifier)
  {
  case 0x1:
    return RIGHT_TRIANGLE;
  case 0x2:
    return GENERAL_TRIANGLE;
  case 0x3:
    return UP_ARROW;
  case 0x4:
    return STAR;
  case 0x5:
    return HEART;
  case 0x6:
    return ISOCELES_TRIANGLE;
  case 0x7:
    return PARALLELOGRAM;
  case 0x8:
    return TILTED_TRAPEZOID;
  case 0x9:
    return UP_DOWN_ARROW;
  case 0xA:
    return SEAL_16;
  case 0xB:
    return WAVE;
  case 0xC:
    return DIAMOND;
  case 0xD:
    return TRAPEZOID;
  case 0xE:
    return CHEVRON_UP;
  case 0xF:
    return BENT_ARROW;
  case 0x10:
    return SEAL_24;
  case 0x11:
    return PIE;
  case 0x12:
    return PENTAGON;
  case 0x13:
    return PENTAGON_UP;
  case 0x14:
    return NOTCHED_TRIANGLE;
  case 0x15:
    return U_TURN_ARROW;
  case 0x16:
    return IRREGULAR_SEAL_1;
  case 0x17:
    return CHORD;
  case 0x18:
    return HEXAGON;
  case 0x19:
    return NOTCHED_RECTANGLE;
  case 0x1A:
    return W_SHAPE; //This is a bizarre shape; the number of vertices depends on one of the adjust values.
  //We need to refactor our escher shape drawing routines before we can handle it.
  case 0x1B:
    return ROUND_RECT_CALLOUT_2K; //This is not quite the same as the round rect. found in 2k2 and above.
  case 0x1C:
    return IRREGULAR_SEAL_2;
  case 0x1D:
    return BLOCK_ARC_2;
  case 0x1E:
    return OCTAGON;
  case 0x1F:
    return PLUS;
  case 0x20:
    return CUBE;
  case 0x21:
    return OVAL_CALLOUT_2K; //Not sure yet if this is the same as the 2k2 one.
  case 0x22:
    return LIGHTNING_BOLT;
  case 0x23:
    return MOON_2;
  default:
    return UNKNOWN_SHAPE;
  }
}

void MSPUBParser2k::parseContentsTextIfNecessary(librevenge::RVNGInputStream *)
{
}

void MSPUBParser2k::parseBulletDefinitions(const ContentChunkReference &, librevenge::RVNGInputStream *)
{
}

void MSPUBParser2k::parseTextInfos(const ContentChunkReference &, librevenge::RVNGInputStream *)
{
}

void MSPUBParser2k::parseTableInfoData(librevenge::RVNGInputStream *input, unsigned seqNum, ChunkHeader2k const &header,
                                       unsigned, unsigned numCols, unsigned numRows, unsigned width, unsigned height)
{
  if (!numRows || !numCols || numRows>128 || numCols>128)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseTableInfoData: unexpected number of rows/columns\n"));
    return;
  }
  TableInfo ti(numRows, numCols);
  if (header.hasData())
  {
    input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
    ListHeader2k listHeader;
    if (!parseListHeader(input, header.m_endOffset, listHeader, false) || listHeader.m_dataSize!=8 || listHeader.m_N<numCols+numRows)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseTableInfoData: can not read the data zone\n"));
    }
    else
    {
      for (int wh=0; wh<2; ++wh)
      {
        unsigned num=wh==0 ? numCols : numRows;
        std::vector<unsigned> &sizes=wh==0 ? ti.m_columnWidthsInEmu : ti.m_rowHeightsInEmu;
        sizes.reserve(size_t(num));
        for (unsigned i=0; i<num; ++i)
        {
          input->seek(4, librevenge::RVNG_SEEK_CUR); // cumulated size
          sizes.push_back(readU32(input));
        }
      }
    }
  }
  else
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseTableInfoData: can not find the data zone\n"));
  }
  // just in case we have no read all row/columns
  ti.m_rowHeightsInEmu.resize(size_t(numRows),height/numRows);
  ti.m_columnWidthsInEmu.resize(size_t(numCols),width/numCols);

  // TODO find the overriden/diagonal cell...
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

void MSPUBParser2k::parseClipPath(librevenge::RVNGInputStream *, unsigned, ChunkHeader2k const &)
{
}

bool MSPUBParser2k::parseContents(librevenge::RVNGInputStream *input)
{
  input->seek(0x16, librevenge::RVNG_SEEK_SET);
  unsigned trailerOffset = readU32(input);
  input->seek(trailerOffset, librevenge::RVNG_SEEK_SET);
  unsigned numBlocks = readU16(input);
  std::set<unsigned> offsetsSet;
  boost::optional<unsigned> bulletChunkIndex;
  boost::optional<unsigned> textInfoChunkIndex;
  for (unsigned i = 0; i < numBlocks; ++i)
  {
    input->seek(input->tell() + 2, librevenge::RVNG_SEEK_SET);
    unsigned short id = readU16(input);
    unsigned short parent = readU16(input);
    auto chunkOffset = readU32(input);
    offsetsSet.insert(chunkOffset);
    unsigned offset = unsigned(input->tell());
    input->seek(chunkOffset, librevenge::RVNG_SEEK_SET);
    unsigned short typeMarker = readU16(input);
    input->seek(offset, librevenge::RVNG_SEEK_SET);
    unsigned const chunkId=unsigned(m_contentChunks.size());
    m_chunkChildIndicesById[parent].push_back(chunkId);
    m_fileIdToChunkId[id]=chunkId;
    switch (typeMarker)
    {
    case 0x0014:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found page chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(PAGE, chunkOffset, 0, id, parent));
      m_pageChunkIndices.push_back(chunkId);
      break;
    case 0x0015:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found document chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(DOCUMENT, chunkOffset, 0, id, parent));
      m_documentChunkIndex = chunkId;
      break;
    case 0x0016:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found text info chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(TEXT_INFO, chunkOffset, 0, id, parent));
      textInfoChunkIndex=chunkId;
      break;
    case 0x001e:
      m_contentChunks.push_back(ContentChunkReference(FONT, chunkOffset, 0, id, parent));
      m_fontChunkIndices.push_back(chunkId);
      break;
    case 0x0001: // table in Contents
    case 0x000a: // table in Quill
      m_contentChunks.push_back(ContentChunkReference(TABLE, chunkOffset, 0, id, parent));
      m_shapeChunkIndices.push_back(chunkId);
      break;
    case 0x0002:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found image_2k chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(IMAGE_2K, chunkOffset, 0, id, parent));
      m_shapeChunkIndices.push_back(chunkId);
      break;
    case 0x0003:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found ole chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(OLE_2K, chunkOffset, 0, id, parent));
      m_shapeChunkIndices.push_back(chunkId);
      break;
    case 0x0021:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found image_2k_data chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(IMAGE_2K_DATA, chunkOffset, 0, id, parent));
      m_imageDataChunkIndices.push_back(chunkId);
      break;
    case 0x0022:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found ole_2k_data chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(OLE_2K_DATA, chunkOffset, 0, id, parent));
      m_oleDataChunkIndices.push_back(chunkId);
      break;
    case 0x0000: // text in Contents
    case 0x0004: // line
    case 0x0005: // rect
    case 0x0006: // shape
    case 0x0007: // ellipe
    case 0x0008: // text in Quill
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found shape chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(SHAPE, chunkOffset, 0, id, parent));
      m_shapeChunkIndices.push_back(chunkId);
      break;
    case 0x0047:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found palette chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(PALETTE, chunkOffset, 0, id, parent));
      m_paletteChunkIndices.push_back(chunkId);
      break;
    case 0x001F:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found borderArt chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(BORDER_ART, chunkOffset, 0, id, parent));
      m_borderArtChunkIndices.push_back(chunkId);
      break;
    case 0x000E:
    case 0x000F:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found group chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(GROUP, chunkOffset, 0, id, parent));
      m_shapeChunkIndices.push_back(chunkId);
      break;
    case 0x0027:
      // CHECKME: v98: the zone data contains the picture is not
      // included in the file, the file name still appears in the zone
      // data....
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found special paper chunk 0x%x, ignored\n", id));
      m_contentChunks.push_back(ContentChunkReference(UNKNOWN_CHUNK, chunkOffset, 0, id, parent));
      m_specialPaperChunkIndex=id;
      break;
    case 0x0028:
      m_contentChunks.push_back(ContentChunkReference(BULLET_DEFINITION, chunkOffset, 0, id, parent));
      bulletChunkIndex=chunkId;
      break;
    default:
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:Found unknown chunk of id 0x%x and parent 0x%x\n", id, parent));
      m_contentChunks.push_back(ContentChunkReference(UNKNOWN_CHUNK, chunkOffset, 0, id, parent));
      m_unknownChunkIndices.push_back(chunkId);
      break;
    }
  }

  // time to update the chunk limit
  std::set<unsigned> zonesLimit;
  input->seek(0x8, librevenge::RVNG_SEEK_SET);
  zonesLimit.insert(readU32(input));
  input->seek(0x12, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<3; ++i) zonesLimit.insert(readU32(input));
  auto zIt=zonesLimit.find(trailerOffset);
  if (zIt!=zonesLimit.end()) ++zIt;
  if (zIt!=zonesLimit.end()) offsetsSet.insert(*zIt);
  for (auto &chunk : m_contentChunks)
  {
    auto oIt=offsetsSet.find(unsigned(chunk.offset));
    if (oIt!=offsetsSet.end()) ++oIt;
    if (oIt!=offsetsSet.end())
      chunk.end=*oIt;
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:can not find limit for chunk %x.\n", chunk.seqNum));
    }
  }

  // parse the text bullet and the text content
  if (bulletChunkIndex)
    parseBulletDefinitions(m_contentChunks.at(*bulletChunkIndex), input);
  if (textInfoChunkIndex)
    parseTextInfos(m_contentChunks.at(*textInfoChunkIndex), input);
  parseContentsTextIfNecessary(input);
  // parse the meta data
  parseMetaData();

  // parse the main chunk
  if (!parseDocument(input))
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents:No document chunk found.\n"));
    return false;
  }
  parseFonts(input);

  for (unsigned int paletteChunkIndex : m_paletteChunkIndices)
  {
    const ContentChunkReference &chunk = m_contentChunks.at(paletteChunkIndex);
    input->seek(long(chunk.offset), librevenge::RVNG_SEEK_SET);
    input->seek(0xA0, librevenge::RVNG_SEEK_CUR);
    for (unsigned j = 0; j < 8; ++j)
    {
      unsigned hex = readU32(input);
      Color color = getColorBy2kHex(hex);
      m_collector->addPaletteColor(color);
    }
  }
  parseBorderArts(input);
  for (unsigned int imageDataChunkIndex : m_imageDataChunkIndices)
  {
    const ContentChunkReference &chunk = m_contentChunks.at(imageDataChunkIndex);
    input->seek(long(chunk.offset) + 4, librevenge::RVNG_SEEK_SET);
    unsigned toRead = readU32(input);
    librevenge::RVNGBinaryData img;
    readData(input, toRead, img);
    m_collector->addImage(++m_lastAddedImage, WMF, img);
    if (m_specialPaperChunkIndex && chunk.parentSeqNum==*m_specialPaperChunkIndex)
    {
      m_collector->setShapeFill(chunk.parentSeqNum,
                                std::make_shared<ImgFill>(m_lastAddedImage, m_collector, false, 0),
                                true);
    }
    else
      m_collector->setShapeImgIndex(chunk.parentSeqNum, m_lastAddedImage);
  }
  std::set<unsigned> oleIds;
  for (unsigned int oleDataChunkIndex : m_oleDataChunkIndices)
  {
    const ContentChunkReference &chunk = m_contentChunks.at(oleDataChunkIndex);
    input->seek(long(chunk.offset), librevenge::RVNG_SEEK_SET);
    ChunkHeader2k header;
    parseChunkHeader(chunk,input,header);
    if (header.hasData())
    {
      input->seek(header.m_dataOffset+2, librevenge::RVNG_SEEK_SET);
      unsigned id = readU32(input);
      m_collector->setShapeOLEIndex(chunk.parentSeqNum, id);
      oleIds.insert(id);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents: can not read OLE data %x.\n", chunk.parentSeqNum));
    }
  }
  if (!oleIds.empty())
  {
    OLEParser oleParser;
    oleParser.parse(m_input);
    auto const &objectMap=oleParser.getObjectsMap();
    for (auto id : oleIds)
    {
      auto it=objectMap.find(int(id));
      if (it==objectMap.end())
      {
        MSPUB_DEBUG_MSG(("MSPUBParser2k::parseContents: can not find OLE %d.\n", int(id)));
        continue;
      }
      m_collector->addOLE(id, it->second);
    }
  }

  for (unsigned int shapeChunkIndex : m_shapeChunkIndices)
  {
    auto const &chunk=m_contentChunks.at(shapeChunkIndex);
    if (m_shapesAlreadySend.find(chunk.seqNum)!=m_shapesAlreadySend.end())
      continue;
    parse2kShapeChunk(chunk, input);
  }

  return true;
}

bool MSPUBParser2k::parseDocument(librevenge::RVNGInputStream *input)
{
  if (bool(m_documentChunkIndex))
  {
    auto const &chunk=m_contentChunks[m_documentChunkIndex.get()];
    ChunkHeader2k header;
    parseChunkHeader(chunk,input,header);
    // changeme: we must find the version before...
    if (m_version==5 && header.m_maxHeaderSize>0xd2) // 2: 5e, 3: 78, 97: 9e, 98: d2, 2000: de
      m_version=6;
    else if (m_version==3 && header.m_maxHeaderSize==0x9e)
      m_version=4;
    if (header.headerLength()>=28)   // size 54|6a|b2
    {
      input->seek(header.m_beginOffset+0x12, librevenge::RVNG_SEEK_SET);
      unsigned short coordinateSystemMark = readU16(input);
      m_isBanner = coordinateSystemMark == 0x0007;
      unsigned width = readU32(input);
      unsigned height = readU32(input);
      m_collector->setWidthInEmu(width);
      m_collector->setHeightInEmu(height);
      // FINISHME: read the end of this field
      // v3: size=106, in pos=68: paper special id[type=27], type=23, type=31, font, art, type=16, ...
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseDocument: the header is too short\n"));
    }
    if (header.hasData())
    {
      input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
      std::vector<unsigned> pages;
      if (parseIdList(input, chunk.end, pages) && pages.size()>=2)
      {
        unsigned masterId=pages[1];
        unsigned numExtras = m_version == 2 ? 0 : m_version <=4 ? 1 : m_version==5 ? 3 : 0;
        // v3,97 : followed by one page(default page?)
        // 98: followed by three pages(default first page, even page, odd page?)
        auto numPages=pages.size();
        for (unsigned extra=1; extra<=numExtras; ++extra)
        {
          if (numPages<extra) break;
          unsigned page=unsigned(numPages-extra);
          if (page<3) break; // keep at least layout, background, first page
          if (m_chunkChildIndicesById.find(pages[page])!=m_chunkChildIndicesById.end())
          {
            MSPUB_DEBUG_MSG(("MSPUBParser2k::parseDocument: find a not empty extra page=%d\n", int(page)));
            continue; // this page contain some data, unsure if we need to keep it
          }
          pages.erase(pages.begin()+int(page));
        }
        for (size_t i=2; i<pages.size(); ++i)
          m_collector->addPage(pages[i]);
        if (m_version<=5)
        {
          m_collector->addPage(masterId);
          m_collector->designateMasterPage(masterId);
          if (m_specialPaperChunkIndex)
            m_collector->setPageBgShape(masterId, *m_specialPaperChunkIndex);
          parsePage(input, masterId);
          for (size_t i=2; i<pages.size(); ++i)
          {
            auto p = pages[i];
            m_collector->setNextPage(p);
            m_collector->setMasterPage(p, masterId);
            parsePage(input, p);
          }
        }
      }
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseDocument: can not find the page list\n"));
    }
    return true;
  }
  return false;
}

bool MSPUBParser2k::parsePage(librevenge::RVNGInputStream *input, unsigned seqNum)
{
  ContentChunkReference chunk;
  if (!getChunkReference(seqNum, chunk))
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parsePage: can not find the page %x\n", seqNum));
    return false;
  }
  ChunkHeader2k header;
  parseChunkHeader(chunk,input,header);
  if (!header.hasData())
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parsePage: can not find the page list %x\n", seqNum));
    return false;
  }
  input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
  std::vector<unsigned> ids;
  if (!parseIdList(input, chunk.end, ids))
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parsePage: can not read the page list %x\n", seqNum));
    return false;
  }
  for (auto cId : ids)
  {
    ContentChunkReference cChunk;
    if (!getChunkReference(cId, cChunk))
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parsePage: can not find child=%x in the page %x\n", cId, seqNum));
      continue;
    }
    parse2kShapeChunk(cChunk, input, seqNum, false);
  }
  return true;
}

bool MSPUBParser2k::parseFonts(librevenge::RVNGInputStream *input)
{
  // checkme: this code must also work in 98, 2000 but there is also a font list in the Quill stream...
  if (m_version>=5) return true;
  for (auto id : m_fontChunkIndices)
  {
    auto const &chunk=m_contentChunks[id];
    ChunkHeader2k header;
    parseChunkHeader(chunk,input,header);
    if (!header.hasData())
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseFonts: can not find the data block %x\n", id));
      continue;
    }
    ListHeader2k listHeader;
    input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
    if (!parseListHeader(input, chunk.end, listHeader, true))
      continue;
    auto const &pos=listHeader.m_positions;
    for (size_t i=0; i+1<pos.size(); ++i)
    {
      std::vector<unsigned char> name;
      if (pos[i]+2>=pos[i+1] || pos[i+1]>chunk.end)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser2k::parseFonts: can not read name %d in the data block %x\n", int(i), id));
        m_collector->addFont(name);
        continue;
      }
      input->seek(pos[i]+2, librevenge::RVNG_SEEK_SET);
      if (m_version<5)
      {
        for (auto l=pos[i]+2; l<pos[i+1]; ++l)
        {
          auto ch=readU8(input);
          if (ch==0)
          {
            if (l+1==pos[i+1])
              break;
            MSPUB_DEBUG_MSG(("MSPUBParser2k::parseFonts: find unexpected 0 in name %d in the data block %x\n", int(i), id));
            name.clear();
            break;
          }
          name.push_back(ch);
        }
      }
      else if (pos[i]+4<pos[i+1])
        readNBytes(input, pos[i+1]-pos[i]-4, name);
      m_collector->addFont(name);
    }
  }
  return true;
}
bool MSPUBParser2k::parseBorderArts(librevenge::RVNGInputStream *input)
{
  if (m_borderArtChunkIndices.size()!=1)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseBorderArts: unexpected number of border arts\n"));
    return false;
  }
  auto const &chunk=m_contentChunks[m_borderArtChunkIndices[0]];
  ChunkHeader2k header;
  parseChunkHeader(chunk,input,header);
  if (!header.hasData())
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseBorderArts: can not find the data block\n"));
    return false;
  }
  if (m_version>=6) // FIXME
    return true;
  input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
  ListHeader2k listHeader;
  if (!parseListHeader(input, chunk.end, listHeader, true))
    return false;
  std::set<unsigned> listPos(listHeader.m_positions.begin(), listHeader.m_positions.end());
  listPos.insert(header.m_endOffset);
  for (size_t p=0; p+1<listHeader.m_positions.size(); ++p)
  {
    if (listHeader.m_positions[p]>=header.m_endOffset) continue;
    auto it=listPos.find(listHeader.m_positions[p]);
    if (it==listPos.end() || ++it==listPos.end())
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseBorderArts: can not find the end position for art=%d\n",int(p)));
      continue;
    }
    input->seek(listHeader.m_positions[p], librevenge::RVNG_SEEK_SET);
    parseBorderArt(input, unsigned(p), *it);
  }
  return true;
}

bool MSPUBParser2k::parseBorderArt(librevenge::RVNGInputStream *input, unsigned borderNum, unsigned endPos)
{
  auto begPos=input->tell();
  unsigned const headerSize=m_version<5 ? 50 : 92;
  if (unsigned(begPos+headerSize+16+4)>endPos)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseBorderArt: art zone %d seems to short\n",int(borderNum)));
    return false;
  }
  input->seek(begPos+headerSize, librevenge::RVNG_SEEK_SET);
  unsigned decal[8];
  for (auto &d : decal) d = readU16(input);
  std::map<unsigned, unsigned> offsetToImage;
  for (int off=0; off<8; ++off)
  {
    auto oIt=offsetToImage.find(decal[off]);
    if (oIt!=offsetToImage.end())
    {
      m_collector->setBorderImageOffset(borderNum,oIt->second);
      continue;
    }
    input->seek(begPos+decal[off], librevenge::RVNG_SEEK_SET);
    // check that we have a picture
    unsigned headerVals[2];
    for (auto &val : headerVals) val=readU16(input);
    if (headerVals[0]<1 || headerVals[0]>2 || headerVals[1]<9 || headerVals[1]>10)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseBorderArt: can not find the wmf picture for art zone %d\n",int(borderNum)));
      continue;
    }
    input->seek(2, librevenge::RVNG_SEEK_CUR);
    unsigned pictSize=readU32(input);
    if (pictSize<9 || begPos+decal[off]+2*pictSize>endPos)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseBorderArt: art zone %d pictSize seems bad\n",int(borderNum)));
      continue;
    }
    // ok, let save the picure
    pictSize*=2;
    input->seek(begPos+decal[off], librevenge::RVNG_SEEK_SET);
    librevenge::RVNGBinaryData &img = *(m_collector->addBorderImage(WMF, borderNum));
    readData(input, pictSize, img);
    unsigned newId=unsigned(offsetToImage.size());
    m_collector->setBorderImageOffset(borderNum,newId);
    if (off==0) m_collector->setShapeStretchBorderArt(borderNum);
    offsetToImage[decal[off]]=newId;
  }
  return true;
}

bool MSPUBParser2k::parseIdList(librevenge::RVNGInputStream *input, unsigned long endPos, std::vector<unsigned> &ids)
{
  ListHeader2k listHeader;
  if (!parseListHeader(input, endPos, listHeader, false) || listHeader.m_dataSize!=2)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseIdList: can not read a list\n"));
    return false;
  }
  ids.reserve(size_t(listHeader.m_N));
  for (unsigned id=0; id<listHeader.m_N; ++id) ids.push_back(readU16(input));
  return true;
}

bool MSPUBParser2k::parseListHeader(librevenge::RVNGInputStream *input, unsigned long endPos, ListHeader2k &header, bool readPosition)
{
  unsigned start=(unsigned)input->tell();
  if (start+10>endPos)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseListHeader: the zone seems too short\n"));
    return false;
  }
  header.m_dataOffset=start+10;
  header.m_N=readU16(input);
  header.m_maxN=readU16(input);
  if (header.m_maxN<header.m_N)
  {
    input->seek(start,librevenge::RVNG_SEEK_SET);
    header.m_N=readU32(input);
    header.m_maxN=readU32(input);
    if (start+18>endPos || header.m_maxN<header.m_N)
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseListHeader: the header seems corrupted\n"));
      return false;
    }
    header.m_pointerSize=4;
    header.m_dataOffset=start+18;
  }
  if (!readPosition)
    header.m_dataSize=header.m_pointerSize==4 ? readU32(input) : readU16(input);
  else
    input->seek(header.m_pointerSize, librevenge::RVNG_SEEK_CUR);
  header.m_values[0]=readU16(input);
  header.m_values[1]=header.m_pointerSize==4 ? readS32(input) : readS16(input);
  if ((header.m_dataSize && (endPos-header.m_dataOffset)/unsigned(header.m_dataSize) < unsigned(header.m_N)) ||
      (readPosition && endPos-header.m_dataOffset<header.m_pointerSize*unsigned(header.m_N+1)))
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseListHeader: problem with m_N\n"));
    return false;
  }
  if (!readPosition)
    return true;
  header.m_positions.resize(size_t(header.m_N+1));
  for (auto &p : header.m_positions) p=header.m_dataOffset+(header.m_pointerSize==4 ? readU32(input) : readU16(input));
  return true;
}

void MSPUBParser2k::parseChunkHeader(ContentChunkReference const &chunk, librevenge::RVNGInputStream *input,
                                     ChunkHeader2k &header)
{
  auto const &chunkOffset=chunk.offset;
  input->seek(long(chunkOffset), librevenge::RVNG_SEEK_SET);
  header.m_beginOffset=unsigned(chunk.offset);
  header.m_fileType=readU16(input);
  header.m_endOffset=unsigned(chunk.end);
  switch (header.m_fileType)
  {
  case 0: // old text in Contents
  case 8: // new text in Quill
    m_collector->setShapeType(chunk.seqNum, RECTANGLE);
    header.m_type=C_Text;
    break;
  case 1: // table in Contents
  case 0xa: // table in Quill
    header.m_type=C_Table;
    break;
  case 2:
    header.m_type=C_Image;
    break;
  case 3:
    header.m_type=C_OLE;
    break;
  case 4:
    header.m_type=C_Line;
    header.m_flagOffset = 0x41;
    m_collector->setShapeType(chunk.seqNum, LINE);
    break;
  case 5:
    header.m_type=C_Rect;
    m_collector->setShapeType(chunk.seqNum, RECTANGLE);
    break;
  case 6:
    header.m_type=C_CustomShape;
    header.m_flagOffset = 0x33;
    break;
  case 7:
    header.m_type=C_Ellipse;
    m_collector->setShapeType(chunk.seqNum, ELLIPSE);
    break;
  case 0xe:
  case 0xf:
    header.m_type=C_Group;
    break;
  case 0x14:
    header.m_type=C_Page;
    break;
  case 0x15:
    header.m_type=C_Document;
    break;
  default:
    break;
  }
  input->seek(long(chunkOffset)+2, librevenge::RVNG_SEEK_SET);
  header.m_maxHeaderSize=readU8(input);
  header.m_dataOffset=unsigned(chunkOffset+readU8(input));
}

bool MSPUBParser2k::parse2kShapeChunk(const ContentChunkReference &chunk, librevenge::RVNGInputStream *input,
                                      boost::optional<unsigned> pageSeqNum, bool topLevelCall)
{
  if (m_shapesAlreadySend.find(chunk.seqNum)!=m_shapesAlreadySend.end())
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parse2kShapeChunk: chunk %u is already parsed\n", chunk.seqNum));
    return false;
  }
  m_shapesAlreadySend.insert(chunk.seqNum);

  unsigned page = pageSeqNum.get_value_or(chunk.parentSeqNum);
  input->seek(long(chunk.offset), librevenge::RVNG_SEEK_SET);
  if (topLevelCall && m_version>5)
  {
    // ignore non top level shapes
    int i_page = -1;
    for (unsigned int pageIndex : m_pageChunkIndices)
    {
      if (m_contentChunks.at(pageIndex).seqNum == chunk.parentSeqNum)
      {
        i_page = int(pageIndex);
        break;
      }
    }
    if (i_page == -1)
    {
      return false;
    }
    if (getPageTypeBySeqNum(m_contentChunks[size_t(i_page)].seqNum) != NORMAL)
    {
      return false;
    }
    // if the parent of this is a page and hasn't yet been added, then add it.
    if (!m_collector->hasPage(chunk.parentSeqNum))
    {
      m_collector->addPage(chunk.parentSeqNum);
    }
  }
  m_collector->setShapePage(chunk.seqNum, page);
  m_collector->setShapeBorderPosition(chunk.seqNum, INSIDE_SHAPE); // This appears to be the only possibility for MSPUB2k
  ChunkHeader2k header;
  parseChunkHeader(chunk, input, header);
  if (m_version>=3)
  {
    // shape transforms are NOT compounded with group transforms. They are equal to what they would be
    // if the shape were not part of a group at all. This is different from how MSPUBCollector handles rotations;
    // we work around the issue by simply not setting the rotation of any group, thereby letting it default to zero.
    //
    // Furthermore, line rotations are redundant and need to be treated as zero.
    unsigned short counterRotationInDegreeTenths = readU16(input);
    if (header.m_type!=C_Group && header.m_type!=C_Line)
      m_collector->setShapeRotation(chunk.seqNum, 360. - double(counterRotationInDegreeTenths) / 10);
  }
  int xs = translateCoordinateIfNecessary(readS32(input));
  int ys = translateCoordinateIfNecessary(readS32(input));
  int xe = translateCoordinateIfNecessary(readS32(input));
  int ye = translateCoordinateIfNecessary(readS32(input));
  m_collector->setShapeCoordinatesInEmu(chunk.seqNum, xs, ys, xe, ye);
  parseShapeFormat(input, chunk.seqNum, header);
  if (header.m_type==C_Group)
    return parseGroup(input, chunk.seqNum, page);
  m_collector->setShapeOrder(chunk.seqNum);
  return true;
}

void MSPUBParser2k::parseShapeFormat(librevenge::RVNGInputStream *input, unsigned seqNum,
                                     ChunkHeader2k const &header)
{
  if (m_version>=5 && (m_version>5 || (header.m_fileType>8 && header.m_fileType!=0xa)))
  {
    // REMOVEME: old code
    parseShapeFlips(input, header.m_flagOffset, seqNum, header.m_beginOffset);
    if (header.m_type==C_Group) // checkme
      return;
    if (header.m_type == C_Text)
    {
      input->seek(header.m_beginOffset + getTextIdOffset(), librevenge::RVNG_SEEK_SET);
      unsigned txtId = readU16(input);
      m_collector->addTextShape(txtId, seqNum);
    }
    if (header.m_type==C_CustomShape)
    {
      input->seek(header.m_beginOffset + 0x31, librevenge::RVNG_SEEK_SET);
      ShapeType shapeType = getShapeType(readU8(input));
      if (shapeType != UNKNOWN_SHAPE)
        m_collector->setShapeType(seqNum, shapeType);
    }
    if (header.m_type!=C_Image)
      parseShapeFill(input, seqNum, header.m_beginOffset);
    parseShapeLine(input, header.isRectangle(), header.m_beginOffset, seqNum);
    return;
  }
  if (header.m_type==C_Group)
    return;
  if (unsigned(input->tell())+(m_version==2 ? 9 : m_version<5 ? 19 : m_version==5 ? 27 : 29)>header.m_dataOffset)
  {
    MSPUB_DEBUG_MSG(("MSPUBParser2k::parseShapeFormat: the zone is too small\n"));
    return;
  }
  unsigned headerFlags=readU16(input); // flags: 1=shadow, 4=selected
  if (headerFlags&0x18)
    m_collector->setShapeWrapping(seqNum, ShapeInfo::W_Dynamic);
  if (m_version>=5) input->seek(8, librevenge::RVNG_SEEK_CUR);
  if (m_version>=6) input->seek(2, librevenge::RVNG_SEEK_CUR);
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
    widths[0]=double(translateLineWidth(w))/4;
  }
  else
  {
    auto w=readU8(input);
    widths[0]=double(translateLineWidth(w))/4;
    bColors[0]=readU32(input);
  }
  input->seek(2,librevenge::RVNG_SEEK_CUR); // 0
  int borderId=0xfffe; // none
  if (header.isRectangle() && unsigned(input->tell())+(m_version==2 ? 9 : 21)<=header.m_dataOffset)
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
        widths[j]=double(translateLineWidth(w))/4;
      }
    }
    else
    {
      for (int j=1; j<4; ++j)
      {
        input->seek(1,librevenge::RVNG_SEEK_CUR);
        auto w=readU8(input);
        widths[j]=double(translateLineWidth(w))/4;
        bColors[j]=readU32(input);
      }
    }
    if (header.m_fileType == 0 && unsigned(input->tell())+11<=header.m_dataOffset)
    {
      // text in Contents
      input->seek(8, librevenge::RVNG_SEEK_CUR); // margin?
      unsigned txtId = 65536+readU16(input);
      m_collector->addTextShape(m_chunkIdToTextEndMap.find(seqNum)!=m_chunkIdToTextEndMap.end() ? seqNum : txtId, seqNum);
      auto fl=readU8(input);
      if ((fl>>4)!=1)
      {
        MSPUB_DEBUG_MSG(("MSPUBParser2k::parseShapeFormat: find %d columns for zone %x\n", int(fl>>4), seqNum));
      }
    }
    else if (header.m_fileType == 8 && unsigned(input->tell())+20<=header.m_dataOffset)
    {
      // text in Quill
      input->seek(10+4+4, librevenge::RVNG_SEEK_CUR); // 5:margin, 0, color, numCol
      unsigned txtId = readU16(input);
      m_collector->addTextShape(txtId, seqNum);
    }
    else if (header.m_fileType == 1 && unsigned(input->tell())+(m_version==2 ? 24 : 32)<=header.m_dataOffset)
    {
      // table in Contents
      input->seek(8, librevenge::RVNG_SEEK_CUR); // margin?
      unsigned txtId = 65536+readU16(input);
      if (m_chunkIdToTextEndMap.find(seqNum)!=m_chunkIdToTextEndMap.end()) txtId=seqNum;
      m_collector->addTextShape(txtId, seqNum);
      input->seek(2, librevenge::RVNG_SEEK_CUR); // data size ?
      unsigned numCols = readU16(input);
      if (m_version>2) input->seek(4, librevenge::RVNG_SEEK_CUR); // 0?
      unsigned numRows = readU16(input);
      if (m_version>2) input->seek(4, librevenge::RVNG_SEEK_CUR); // 0?
      unsigned width=readU32(input);
      unsigned height=readU32(input);
      if (numRows && numCols)
        parseTableInfoData(input, seqNum, header, txtId, numCols, numRows, width, height);
    }
    else if (header.m_fileType == 0xa && unsigned(input->tell())+32<=header.m_dataOffset)
    {
      // table in Quill
      input->seek(10, librevenge::RVNG_SEEK_CUR); // margin? + ?
      unsigned numCols = readU16(input);
      input->seek(4, librevenge::RVNG_SEEK_CUR); // color
      unsigned numRows = readU16(input);
      input->seek(4, librevenge::RVNG_SEEK_CUR); // color
      unsigned width=readU32(input);
      unsigned height=readU32(input);
      input->seek(2, librevenge::RVNG_SEEK_CUR); // unknown
      unsigned txtId = readU16(input);
      m_collector->addTextShape(txtId, seqNum);
      if (numRows && numCols)
        parseTableInfoData(input, seqNum, header, txtId, numCols, numRows, width, height);
    }
    else if (header.m_type==C_Image || header.m_type==C_OLE)
      parseClipPath(input, seqNum, header);
  }
  else if (header.m_type==C_CustomShape && unsigned(input->tell())+12<=header.m_dataOffset)
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
  else if (header.m_type==C_Line && unsigned(input->tell())+18<=header.m_dataOffset)
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
      int wh=i+1 < numBorders ? i+1 : 0;
      m_collector->addShapeLine(seqNum, Line(getColorReferenceByIndex(bColors[wh]), unsigned(widths[wh]*12700), widths[wh]>0));
    }
  }
  else if (widths[0]>0)
  {
    m_collector->addShapeLine(seqNum, Line(getColorReferenceByIndex(bColors[0]), unsigned(widths[0]*12700), true));
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
      gradient->addColor(getColorReferenceByIndex(colors[data.m_swapColor ? 1 : 0]), 0, 1);
      gradient->addColor(getColorReferenceByIndex(colors[data.m_swapColor ? 0 : 1]), 1, 1);
      m_collector->setShapeFill(seqNum, gradient, false);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseShapeFormat: unknown gradiant =%d\n", patternId));
    }
  }
  else if (patternId)
  {
    if (patternId==1 || patternId==2)
      m_collector->setShapeFill(seqNum, std::make_shared<SolidFill>(getColorReferenceByIndex(colors[2-patternId]), 1, m_collector), false);
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
                                 getColorReferenceByIndex(colors[1]),
                                 getColorReferenceByIndex(colors[0])), false);
    }
    else
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseShapeFormat: unknown pattern =%d\n", patternId));
    }
  }
}

unsigned MSPUBParser2k::getShapeFillTypeOffset() const
{
  return 0x2A;
}

unsigned MSPUBParser2k::getShapeFillColorOffset() const
{
  return 0x22;
}

void MSPUBParser2k::parseShapeFill(librevenge::RVNGInputStream *input, unsigned seqNum, unsigned chunkOffset)
{
  input->seek(chunkOffset + getShapeFillTypeOffset(), librevenge::RVNG_SEEK_SET);
  unsigned char fillType = readU8(input);
  if (fillType == 2) // other types are gradients and patterns which are not implemented yet. 0 is no fill.
  {
    input->seek(chunkOffset + getShapeFillColorOffset(), librevenge::RVNG_SEEK_SET);
    unsigned fillColorReference = readU32(input);
    unsigned translatedFillColorReference = translate2kColorReference(fillColorReference);
    m_collector->setShapeFill(seqNum, std::shared_ptr<Fill>(new SolidFill(ColorReference(translatedFillColorReference), 1, m_collector)), false);
  }
}

bool MSPUBParser2k::parseGroup(librevenge::RVNGInputStream *input, unsigned seqNum, unsigned page)
{
  bool retVal = true;
  m_collector->beginGroup();
  m_collector->setCurrentGroupSeqNum(seqNum);
  if (m_version<=5)
  {
    ContentChunkReference chunk;
    if (!getChunkReference(seqNum, chunk))
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseGroup: can not find the group %x\n", seqNum));
      return false;
    }
    ChunkHeader2k header;
    parseChunkHeader(chunk,input,header);
    if (!header.hasData())
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseGroup: can not find the group list %x\n", seqNum));
      return false;
    }
    input->seek(header.m_dataOffset, librevenge::RVNG_SEEK_SET);
    std::vector<unsigned> ids;
    if (!parseIdList(input, chunk.end, ids))
    {
      MSPUB_DEBUG_MSG(("MSPUBParser2k::parseGroup: can not read the group list %x\n", seqNum));
      return false;
    }
    for (auto cId : ids)
    {
      ContentChunkReference cChunk;
      if (!getChunkReference(cId, cChunk))
      {
        MSPUB_DEBUG_MSG(("MSPUBParser2k::parseGroup: can not find child=%x in the group %x\n", cId, seqNum));
        continue;
      }
      parse2kShapeChunk(cChunk, input, page, false);
    }
  }
  else
  {
    const std::map<unsigned, std::vector<unsigned> >::const_iterator it = m_chunkChildIndicesById.find(seqNum);
    if (it != m_chunkChildIndicesById.end())
    {
      const std::vector<unsigned> &chunkChildIndices = it->second;
      for (unsigned int chunkChildIndex : chunkChildIndices)
      {
        const ContentChunkReference &childChunk = m_contentChunks.at(chunkChildIndex);
        retVal = retVal && parse2kShapeChunk(childChunk, input, page, false);
      }
    }
  }
  m_collector->endGroup();
  return retVal;
}

int MSPUBParser2k::translateCoordinateIfNecessary(int coordinate) const
{
  if (m_version>=5)
    return coordinate;
  const int offset = (m_isBanner ? 120 : 25) * EMUS_IN_INCH;
  if (std::numeric_limits<int>::min() + offset > coordinate)
    return std::numeric_limits<int>::min();
  else
    return coordinate - offset;
}

void MSPUBParser2k::parseShapeFlips(librevenge::RVNGInputStream *input, unsigned flagsOffset, unsigned seqNum,
                                    unsigned chunkOffset)
{
  if (flagsOffset)
  {
    input->seek(chunkOffset + flagsOffset, librevenge::RVNG_SEEK_SET);
    unsigned char flags = readU8(input);
    bool flipV = flags & 0x1;
    bool flipH = flags & (0x2 | 0x10); // FIXME: this is a guess
    m_collector->setShapeFlip(seqNum, flipV, flipH);
  }
}

unsigned MSPUBParser2k::getTextIdOffset() const
{
  return 0x58;
}

unsigned MSPUBParser2k::getFirstLineOffset() const
{
  return 0x2C;
}

unsigned MSPUBParser2k::getSecondLineOffset() const
{
  return 0x35;
}

void MSPUBParser2k::parseShapeLine(librevenge::RVNGInputStream *input, bool isRectangle, unsigned offset,
                                   unsigned seqNum)
{
  input->seek(offset + getFirstLineOffset(), librevenge::RVNG_SEEK_SET);
  unsigned char leftLineWidth = readU8(input);
  bool leftLineExists = leftLineWidth != 0;
  unsigned leftColorReference = readU32(input);
  unsigned translatedLeftColorReference = translate2kColorReference(leftColorReference);
  if (isRectangle)
  {
    input->seek(offset + getSecondLineOffset(), librevenge::RVNG_SEEK_SET);
    unsigned char topLineWidth = readU8(input);
    bool topLineExists = topLineWidth != 0;
    unsigned topColorReference = readU32(input);
    unsigned translatedTopColorReference = translate2kColorReference(topColorReference);
    m_collector->addShapeLine(seqNum, Line(ColorReference(translatedTopColorReference),
                                           translateLineWidth(topLineWidth) * EMUS_IN_INCH / (4 * POINTS_IN_INCH), topLineExists));

    input->seek(1, librevenge::RVNG_SEEK_CUR);
    unsigned char rightLineWidth = readU8(input);
    bool rightLineExists = rightLineWidth != 0;
    unsigned rightColorReference = readU32(input);
    unsigned translatedRightColorReference = translate2kColorReference(rightColorReference);
    m_collector->addShapeLine(seqNum, Line(ColorReference(translatedRightColorReference),
                                           translateLineWidth(rightLineWidth) * EMUS_IN_INCH / (4 * POINTS_IN_INCH), rightLineExists));

    input->seek(1, librevenge::RVNG_SEEK_CUR);
    unsigned char bottomLineWidth = readU8(input);
    bool bottomLineExists = bottomLineWidth != 0;
    unsigned bottomColorReference = readU32(input);
    unsigned translatedBottomColorReference = translate2kColorReference(bottomColorReference);
    m_collector->addShapeLine(seqNum, Line(ColorReference(translatedBottomColorReference),
                                           translateLineWidth(bottomLineWidth) * EMUS_IN_INCH / (4 * POINTS_IN_INCH), bottomLineExists));
  }
  m_collector->addShapeLine(seqNum, Line(ColorReference(translatedLeftColorReference),
                                         translateLineWidth(leftLineWidth) * EMUS_IN_INCH / (4 * POINTS_IN_INCH), leftLineExists));
}

bool MSPUBParser2k::parse()
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
  std::unique_ptr<librevenge::RVNGInputStream> quill(m_input->getSubStreamByName("Quill/QuillSub/CONTENTS"));
  if (!quill)
  {
    MSPUB_DEBUG_MSG(("Couldn't get quill stream.\n"));
    return false;
  }
  if (!parseQuill(quill.get()))
  {
    MSPUB_DEBUG_MSG(("Couldn't parse quill stream.\n"));
    return false;
  }
  return m_collector->go();
}

PageType MSPUBParser2k::getPageTypeBySeqNum(unsigned seqNum)
{
  // changeme this make no sense....
  switch (seqNum)
  {
  case 0x116:
  case 0x108:
  case 0x10B:
  case 0x10D:
  case 0x119:
    return DUMMY_PAGE;
  case 0x109:
    return MASTER;
  default:
    return NORMAL;
  }
}

}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
