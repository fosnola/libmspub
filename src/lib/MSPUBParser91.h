/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_MSPUBPARSER91_H
#define INCLUDED_MSPUBPARSER91_H

#include <vector>

#include "MSPUBParser.h"
/* parser for MS Publisher 1 files
 */
namespace libmspub
{
class ColorReference;

struct BlockInfo91;
struct TextPLCHeader91;
struct ZoneHeader91;
struct MSPubParser91Data;

class MSPUBParser91 final : public MSPUBParser
{
public:
  MSPUBParser91(librevenge::RVNGInputStream *input, MSPUBCollector *collector);
  bool parse() final;
protected:
  bool parseContents(librevenge::RVNGInputStream *input) final;
  void parseContentsTextIfNecessary(librevenge::RVNGInputStream *input);

  //! parse the list of block info(zone 4), and recreate the hierarchical structure
  bool parseBlockInfos(librevenge::RVNGInputStream *input);
  //! parse the border arts: zone 6
  bool parseBorderArts(librevenge::RVNGInputStream *input);
  //! parse the document: zone 1
  bool parseDocument(librevenge::RVNGInputStream *input);
  //! parse the list of font name: zone 5
  bool parseFonts(librevenge::RVNGInputStream *input);
  //! parse the list of id by page: zone 3
  bool parsePageIds(librevenge::RVNGInputStream *input);

  //! parse a shape list: subzone 4
  bool parseShapesList(librevenge::RVNGInputStream *input, BlockInfo91 const &info);
  //! parse a shape: subzone 4
  bool parseShape(librevenge::RVNGInputStream *input, BlockInfo91 const &info);
  //! parse an image: subzone 4
  bool parseImage(librevenge::RVNGInputStream *input, BlockInfo91 const &info);

  bool parseZoneHeader(librevenge::RVNGInputStream *input, ZoneHeader91 &header, bool doNotReadPositions);
  bool parseTextPLCHeader(librevenge::RVNGInputStream *input, TextPLCHeader91 &header);
  bool parseSpanStyles(librevenge::RVNGInputStream *input, unsigned index,
                       std::vector<CharacterStyle> &styles, std::map<unsigned, unsigned> &posToStyle);
  bool parseParagraphStyles(librevenge::RVNGInputStream *input, unsigned index,
                            std::vector<ParagraphStyle> &styles, std::map<unsigned, unsigned> &posToStyle);

  //! try to parse a OLE image (only embedded image)
  bool parseOLEPicture(librevenge::RVNGInputStream *input, unsigned length, ImgType &type, librevenge::RVNGBinaryData &img);
  //! try to parse a metafile image
  bool parseMetafilePicture(librevenge::RVNGInputStream *input, unsigned length, ImgType &type, librevenge::RVNGBinaryData &img);

  void translateCoordinateIfNecessary(int &x, int &y) const;
  //! return a color reference corresponding to an id
  ColorReference getColor(int colorId) const;
private:
  std::shared_ptr<MSPubParser91Data> m_data;
};
}

#endif //  INCLUDED_MSPUBPARSER91_H

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
