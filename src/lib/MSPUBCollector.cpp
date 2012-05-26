/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* libmspub
 * Version: MPL 1.1 / GPLv2+ / LGPLv2+
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License or as specified alternatively below. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Major Contributor(s):
 * Copyright (C) 2012 Fridrich Strba <fridrich.strba@bluewin.ch>
 *
 *
 * All Rights Reserved.
 *
 * For minor contributions see the git repository.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPLv2+"), or
 * the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
 * in which case the provisions of the GPLv2+ or the LGPLv2+ are applicable
 * instead of those above.
 */

#include "MSPUBCollector.h"
#include "libmspub_utils.h"
#include "MSPUBConstants.h"
#include "MSPUBTypes.h"

libmspub::MSPUBCollector::MSPUBCollector(libwpg::WPGPaintInterface *painter) :
  m_painter(painter), contentChunkReferences(), m_width(0), m_height(0),
  m_widthSet(false), m_heightSet(false), m_commonPageProperties(),
  m_numPages(0), textStringsById(), pagesBySeqNum(),
  shapesBySeqNum(), images(),
  colors(), defaultColor(0, 0, 0), fonts(),
  defaultCharStyles(), defaultParaStyles(), shapeTypesBySeqNum(),
  possibleImageShapeSeqNums(), shapeImgIndicesBySeqNum(),
  shapeCoordinatesBySeqNum()
{
}

void libmspub::MSPUBCollector::Shape::output(libwpg::WPGPaintInterface *painter, Coordinate coord)
{
  setCoordProps(coord);
  write(painter);
}

void libmspub::MSPUBCollector::Shape::setCoordProps(Coordinate coord)
{
  owner->setRectCoordProps(coord, &props);
}

void libmspub::MSPUBCollector::TextShape::write(libwpg::WPGPaintInterface *painter)
{
  painter->startTextObject(props, WPXPropertyListVector());
  for (unsigned i_lines = 0; i_lines < str.size(); ++i_lines)
  {
    WPXPropertyList paraProps = owner->getParaStyleProps(str[i_lines].style, str[i_lines].style.defaultCharStyleIndex);
    painter->startTextLine(paraProps);
    for (unsigned i_spans = 0; i_spans < str[i_lines].spans.size(); ++i_spans)
    {
      WPXString text;
      appendCharacters(text, str[i_lines].spans[i_spans].chars);
      WPXPropertyList charProps = owner->getCharStyleProps(str[i_lines].spans[i_spans].style, str[i_lines].style.defaultCharStyleIndex);
      painter->startTextSpan(charProps);
      painter->insertText(text);
      painter->endTextSpan();
    }
    painter->endTextLine();
  }
  painter->endTextObject();
}


void libmspub::MSPUBCollector::GeometricShape::setCoordProps(Coordinate coord)
{
  switch (type)
  {
  case ELLIPSE:
    owner->setEllipseCoordProps(coord, &props);
    break;
  case RECTANGLE:
  default:
    owner->setRectCoordProps(coord, &props);
    break;
  };
}

void libmspub::MSPUBCollector::GeometricShape::write(libwpg::WPGPaintInterface *painter)
{
  switch(type)
  {
  case RECTANGLE:
    painter->drawRectangle(props);
    break;
  case ELLIPSE:
    painter->drawEllipse(props);
    break;
  default:
    break;
  }
}

libmspub::MSPUBCollector::ImgShape::ImgShape(const GeometricShape &from, ImgType imgType, WPXBinaryData i, MSPUBCollector *o) : 
  GeometricShape(from.pageSeqNum, o), img(i)
{
  this->type = from.type;
  this->props = from.props;
  setMime_(imgType);
}

void libmspub::MSPUBCollector::ImgShape::setMime_(ImgType imgType)
{
  const char *mime;
  switch (imgType)
  {
  case PNG:
    mime = "image/png";
    break;
  case JPEG:
    mime = "image/jpeg";
    break;
  default:
    mime = "";
    MSPUB_DEBUG_MSG(("Unknown image type %d passed to ImgShape constructor!\n", type));
  }
  props.insert("libwpg:mime-type", mime);
}
void libmspub::MSPUBCollector::ImgShape::write(libwpg::WPGPaintInterface *painter)
{
  painter->drawGraphicObject(props, img);
}

libmspub::MSPUBCollector::~MSPUBCollector()
{
}

bool libmspub::MSPUBCollector::setShapeType(unsigned seqNum, ShapeType type)
{
  return shapeTypesBySeqNum.insert(std::pair<const unsigned, ShapeType>(seqNum, type)).second;
}

void libmspub::MSPUBCollector::setDefaultColor(unsigned char r, unsigned char g, unsigned char b)
{
  defaultColor = Color(r, g, b);
}

void libmspub::MSPUBCollector::addDefaultCharacterStyle(const CharacterStyle &st)
{
  defaultCharStyles.push_back(st);
}

void libmspub::MSPUBCollector::addDefaultParagraphStyle(const ParagraphStyle &st)
{
  defaultParaStyles.push_back(st);
}

bool libmspub::MSPUBCollector::addPage(unsigned seqNum)
{
  if (! (m_widthSet && m_heightSet) )
  {
    return false;
  }
  MSPUB_DEBUG_MSG(("Adding page of seqnum 0x%x\n", seqNum));
  pagesBySeqNum[seqNum] = PageInfo();
  return true;
}

bool libmspub::MSPUBCollector::addTextShape(unsigned stringId, unsigned seqNum, unsigned pageSeqNum)
{
  PageInfo *page = getIfExists(pagesBySeqNum, pageSeqNum);
  if (!page)
  {
    MSPUB_DEBUG_MSG(("Page of seqnum 0x%x not found in addTextShape!\n", pageSeqNum));
    return false;
  }
  else
  {
    std::vector<TextParagraph> *str = getIfExists(textStringsById, stringId);
    if (!str)
    {
      MSPUB_DEBUG_MSG(("Text string of id 0x%x not found in addTextShape!\n", stringId));
      return false;
    }
    else
    {
      if (! ptr_getIfExists(shapesBySeqNum, seqNum))
      {
        shapesBySeqNum.insert(seqNum, new TextShape(*str, this));
        page->shapeSeqNums.push_back(seqNum);
        MSPUB_DEBUG_MSG(("addTextShape succeeded with id 0x%x\n", stringId));
        return true;
      }
      MSPUB_DEBUG_MSG(("already tried to add the text shape of seqnum 0x%x to this page!\n", seqNum));
      return false;
    }
  }
}

bool libmspub::MSPUBCollector::setShapeImgIndex(unsigned seqNum, unsigned index)
{
  MSPUB_DEBUG_MSG(("Setting image index of shape with seqnum 0x%x to 0x%x\n", seqNum, index));
  return shapeImgIndicesBySeqNum.insert(std::pair<const unsigned, unsigned>(seqNum, index)).second;
}

void libmspub::MSPUBCollector::setRectCoordProps(Coordinate coord, WPXPropertyList *props)
{
  int xs = coord.xs, ys = coord.ys, xe = coord.xe, ye = coord.ye;
  double x_center = m_commonPageProperties["svg:width"]->getDouble() / 2;
  double y_center = m_commonPageProperties["svg:height"]->getDouble() / 2;
  props->insert("svg:x", x_center + (double)xs / EMUS_IN_INCH);
  props->insert("svg:y", y_center + (double)ys / EMUS_IN_INCH);
  props->insert("svg:width", (double)(xe - xs) / EMUS_IN_INCH);
  props->insert("svg:height", (double)(ye - ys) / EMUS_IN_INCH);
}

void libmspub::MSPUBCollector::setEllipseCoordProps(Coordinate coord, WPXPropertyList *props)
{
  int xs = coord.xs, ys = coord.ys, xe = coord.xe, ye = coord.ye;
  double x_center = m_commonPageProperties["svg:width"]->getDouble() / 2;
  double y_center = m_commonPageProperties["svg:height"]->getDouble() / 2;
  props->insert("svg:cx", x_center + ((double)xs + (double)xe)/(2 * EMUS_IN_INCH));
  props->insert("svg:cy", y_center + ((double)ys + (double)ye)/(2 * EMUS_IN_INCH));
  props->insert("svg:rx", (double)(xe - xs)/(2 * EMUS_IN_INCH));
  props->insert("svg:ry", (double)(ye - ys)/(2 * EMUS_IN_INCH));
}

bool libmspub::MSPUBCollector::setShapeCoordinatesInEmu(unsigned seqNum, int xs, int ys, int xe, int ye)
{
  return shapeCoordinatesBySeqNum.insert(std::pair<const unsigned, Coordinate>(seqNum, Coordinate(xs, ys, xe, ye))).second;
}

void libmspub::MSPUBCollector::addFont(std::vector<unsigned char> name)
{
  fonts.push_back(name);
}

void libmspub::MSPUBCollector::assignImages()
{
  for (unsigned i = 0; i < possibleImageShapeSeqNums.size(); ++i)
  {
    unsigned *index = getIfExists(shapeImgIndicesBySeqNum, possibleImageShapeSeqNums[i]);
    GeometricShape *shape = (GeometricShape *)ptr_getIfExists(shapesBySeqNum, possibleImageShapeSeqNums[i]);
    if (!shape)
    {
      MSPUB_DEBUG_MSG(("Could not find shape of seqnum 0x%x in assignImages\n", possibleImageShapeSeqNums[i]));
      return;
    }
    if (index && *index - 1 < images.size())
    {
      ImgShape *toInsert = new ImgShape(*shape, images[*index - 1].first, images[*index - 1].second, this);
      shapesBySeqNum.erase(possibleImageShapeSeqNums[i]);
      shapesBySeqNum.insert(possibleImageShapeSeqNums[i], toInsert);
    }
    else
    {
      ShapeType *type = getIfExists(shapeTypesBySeqNum, possibleImageShapeSeqNums[i]);
      if (type)
      {
        shape->type = *type;
      }
      else
      {
        MSPUB_DEBUG_MSG(("Could not find shape type for shape of seqnum 0x%x\n", possibleImageShapeSeqNums[i]));
      }
    }
  }
}

WPXPropertyList libmspub::MSPUBCollector::getParaStyleProps(const ParagraphStyle &style, unsigned defaultParaStyleIndex)
{
  ParagraphStyle _nothing;
  const ParagraphStyle &defaultParaStyle = defaultParaStyleIndex < defaultParaStyles.size() ? defaultParaStyles[defaultParaStyleIndex] : _nothing;
  WPXPropertyList ret;
  Alignment align = style.align >= 0 ? style.align : defaultParaStyle.align;
  switch (align)
  {
  case RIGHT:
    ret.insert("fo:text-align", "right");
    break;
  case CENTER:
    ret.insert("fo:text-align", "center");
    break;
  case JUSTIFY:
    ret.insert("fo:text-align", "justify");
    break;
  case LEFT:
  default:
    ret.insert("fo:text-align", "left");
    break;
  }
  return ret;
}

WPXPropertyList libmspub::MSPUBCollector::getCharStyleProps(const CharacterStyle &style, unsigned defaultCharStyleIndex)
{
  CharacterStyle _nothing = CharacterStyle(false, false, false);
  const CharacterStyle &defaultCharStyle = defaultCharStyleIndex < defaultCharStyles.size() ? defaultCharStyles[defaultCharStyleIndex] : _nothing;
  WPXPropertyList ret;
  if (style.italic)
  {
    ret.insert("fo:font-style", "italic");
  }
  if (style.bold)
  {
    ret.insert("fo:font-weight", "bold");
  }
  if (style.underline)
  {
    ret.insert("style:text-underline-type", "single");
  }
  if (style.textSizeInPt != -1)
  {
    ret.insert("fo:font-size", style.textSizeInPt);
  }
  else if (defaultCharStyle.textSizeInPt != -1)
  {
    ret.insert("fo:font-size", defaultCharStyle.textSizeInPt);
  }
  if (style.colorIndex >= 0 && (size_t)style.colorIndex < colors.size())
  {
    ret.insert("fo:color", getColorString(colors[style.colorIndex]));
  }
  else if (defaultCharStyle.colorIndex >= 0 && (size_t)defaultCharStyle.colorIndex < colors.size())
  {
    ret.insert("fo:color", getColorString(colors[defaultCharStyle.colorIndex]));
  }
  else
  {
    ret.insert("fo:color", getColorString(defaultColor));
  }
  if (style.fontIndex < fonts.size())
  {
    WPXString str;
    appendCharacters(str, fonts[style.fontIndex]);
    ret.insert("style:font-name", str);
  }
  return ret;
}

WPXString libmspub::MSPUBCollector::getColorString(const Color &color)
{
  WPXString ret;
  ret.sprintf("#%.2x%.2x%.2x",(unsigned char)color.r, (unsigned char)color.g, (unsigned char)color.b);
  MSPUB_DEBUG_MSG(("String for r: 0x%x, g: 0x%x, b: 0x%x is %s\n", color.r, color.g, color.b, ret.cstr()));
  return ret;
}

bool libmspub::MSPUBCollector::go()
{
  assignImages();
  for (std::map<unsigned, PageInfo>::const_iterator i = pagesBySeqNum.begin(); i != pagesBySeqNum.end(); ++i)
  {
    const std::vector<unsigned> &shapeSeqNums = i->second.shapeSeqNums; // for readability
    if (shapeSeqNums.size() > 0)
    {
      m_painter->startGraphics(m_commonPageProperties);
      for (unsigned i_seqNums = 0; i_seqNums < shapeSeqNums.size(); ++i_seqNums)
      {
        Shape *shape = ptr_getIfExists(shapesBySeqNum, shapeSeqNums[i_seqNums]);
        if (shape)
        {
          Coordinate *coord = getIfExists(shapeCoordinatesBySeqNum, shapeSeqNums[i_seqNums]);
          if (coord)
          {
            shape->output(m_painter, *coord);
          }
          else
          {
            MSPUB_DEBUG_MSG(("Could not output shape of seqnum 0x%x: no coordinates provided\n", shapeSeqNums[i_seqNums]));
          }
        }
        else
        {
          MSPUB_DEBUG_MSG(("No shape with seqnum 0x%x found in collector\n", shapeSeqNums[i_seqNums]));
        }
      }
      m_painter->endGraphics();
    }
  }
  return true;
}


bool libmspub::MSPUBCollector::addTextString(const std::vector<TextParagraph> &str, unsigned id)
{
  MSPUB_DEBUG_MSG(("addTextString, id: 0x%x\n", id));
  textStringsById[id] = str;
  return true; //FIXME: Warn if the string already existed in the map.
}

void libmspub::MSPUBCollector::setWidthInEmu(unsigned long widthInEmu)
{
  //FIXME: Warn if this is called twice
  m_width = widthInEmu;
  m_commonPageProperties.insert("svg:width", ((double)widthInEmu)/EMUS_IN_INCH);
  m_widthSet = true;
}

void libmspub::MSPUBCollector::setHeightInEmu(unsigned long heightInEmu)
{
  //FIXME: Warn if this is called twice
  m_height = heightInEmu;
  m_commonPageProperties.insert("svg:height", ((double)heightInEmu)/EMUS_IN_INCH);
  m_heightSet = true;
}

bool libmspub::MSPUBCollector::addImage(unsigned index, ImgType type, WPXBinaryData img)
{
  MSPUB_DEBUG_MSG(("Image at index %d and of type 0x%x added.\n", index, type));
  while (images.size() < index)
  {
    images.push_back(std::pair<ImgType, WPXBinaryData>(UNKNOWN, WPXBinaryData()));
  }
  images[index - 1] = std::pair<ImgType, WPXBinaryData>(type, img);
  return true;
}

bool libmspub::MSPUBCollector::addShape(unsigned seqNum, unsigned pageSeqNum)
{
  shapesBySeqNum.insert(seqNum, new GeometricShape(pageSeqNum, this));
  possibleImageShapeSeqNums.push_back(seqNum);
  PageInfo *page = getIfExists(pagesBySeqNum, pageSeqNum);
  if (page)
  {
    page->shapeSeqNums.push_back(seqNum);
  }
  return true;
}

void libmspub::MSPUBCollector::addColor(unsigned char r, unsigned char g, unsigned char b)
{
  colors.push_back(Color(r, g, b));
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
