/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

#include <librevenge/librevenge.h>

#include "libmspub_utils.h"
#include "MSPUBTypes.h"

#include "OLEParser.h"

//////////////////////////////////////////////////
// internal structure
//////////////////////////////////////////////////
namespace libmspub
{
namespace OLEParserInternal
{
/** Internal: internal method to keep ole definition */
struct OleDef
{
  OleDef()
    : m_id(-1)
    , m_base("")
    , m_dir("")
    , m_name("") { }
  int m_id /**main id*/;
  std::string m_base/** the base name*/, m_dir/**the directory*/, m_name/**the complete name*/;
};

/** Internal: the state of a OLEParser */
struct State
{
  //! constructor
  State(std::function<int(std::string const &)> const &dirToIdFunc)
    : m_directoryToIdFunction(dirToIdFunc)
    , m_unknownOLEs()
    , m_idToObjectMap()
  {
  }
  //! the function used to convert a directory name in a id
  std::function<int(std::string const &)> m_directoryToIdFunction;
  //! list of ole which can not be parsed
  std::vector<std::string> m_unknownOLEs;
  //! map id to object
  std::map<int, EmbeddedObject> m_idToObjectMap;
};
}

// constructor/destructor
OLEParser::OLEParser(std::function<int(std::string const &)> const &dirToIdFunc)
  : m_state(new OLEParserInternal::State(dirToIdFunc))
{
}

OLEParser::~OLEParser()
{
}


std::vector<std::string> const &OLEParser::getNotParse() const
{
  return m_state->m_unknownOLEs;
}
//! returns the list of data positions which have been read
std::map<int,EmbeddedObject> const &OLEParser::getObjectsMap() const
{
  return m_state->m_idToObjectMap;
}
// parsing
int OLEParser::getIdFromDirectory(std::string const &dirName)
{
  // try to retrieve the identificator stored in the directory
  //  MatOST/MatadorObject1/ -> 1, -1
  //  Object 2/ -> 2, -1
  auto dir=dirName+'/';
  auto pos = dir.find('/');
  while (pos != std::string::npos)
  {
    if (pos >= 1 && dir[pos-1] >= '0' && dir[pos-1] <= '9')
    {
      auto idP = pos-1;
      while (idP >=1 && dir[idP-1] >= '0' && dir[idP-1] <= '9')
        idP--;
      int val = std::atoi(dir.substr(idP, idP-pos).c_str());
      return val;
    }
    pos = dir.find('/', pos+1);
  }
  MSPUB_DEBUG_MSG(("OLEParser::getIdFromDirectory: can not find id for %s\n", dirName.c_str()));
  return -1;
}

bool OLEParser::parse(librevenge::RVNGInputStream *file)
{
  m_state->m_unknownOLEs.resize(0);
  m_state->m_idToObjectMap.clear();

  if (!file || !file->isStructured()) return false;

  //
  // we begin by grouping the Ole by their potential main id
  //
  std::multimap<int, OLEParserInternal::OleDef> listsById;
  std::vector<int> listIds;
  unsigned numSubStreams = file->subStreamCount();
  for (unsigned i = 0; i < numSubStreams; ++i)
  {
    char const *nm=file->subStreamName(i);
    if (!nm) continue;
    std::string name(nm);
    if (name.empty() || name[name.length()-1]=='/') continue;
    // separated the directory and the name
    //    MatOST/MatadorObject1/Ole10Native
    //      -> dir="MatOST/MatadorObject1", base="Ole10Native"
    auto pos = name.find_last_of('/');

    std::string dir, base;
    if (pos == std::string::npos) base = name;
    else if (pos == 0) base = name.substr(1);
    else
    {
      dir = name.substr(0,pos);
      base = name.substr(pos+1);
    }
    if (dir == "" || dir.substr(0,5)=="Quill") continue;
#ifdef DEBUG
    MSPUB_DEBUG_MSG(("OLEParser::parse: find OLEName=%s\n", name.c_str()));
#endif
    OLEParserInternal::OleDef data;
    data.m_name = name;
    data.m_base = base;
    data.m_dir = dir;
    data.m_id = m_state->m_directoryToIdFunction(dir);
    if (listsById.find(data.m_id) == listsById.end())
      listIds.push_back(data.m_id);
    listsById.insert(std::multimap<int, OLEParserInternal::OleDef>::value_type(data.m_id, data));
  }

  for (auto id : listIds)
  {
    auto pos = listsById.lower_bound(id);

    EmbeddedObject pict;
    while (pos != listsById.end())
    {
      auto const &dOle = pos->second;
      if (pos->first != id) break;
      ++pos;

      librevenge::RVNGInputStream *ole(file->getSubStreamByName(dOle.m_name.c_str()));
      if (!ole)
      {
        MSPUB_DEBUG_MSG(("OLEParser: error: can not find OLE part: \"%s\"\n", dOle.m_name.c_str()));
        continue;
      }

      bool ok = true;
      try
      {
        librevenge::RVNGPropertyList pList;
        if (readOle(ole, dOle.m_base));
        else if (isOlePres(ole, dOle.m_base) && readOlePres(ole, pict));
        else if (isOle10Native(ole, dOle.m_base) && readOle10Native(ole, pict));
        else if (readCompObj(ole, dOle.m_base));
        else if (readContents(ole, dOle.m_base, pict));
        else if (readCONTENTS(ole, dOle.m_base, pict));
        else
          ok = false;
      }
      catch (...)
      {
        ok = false;
      }
      if (!ok)
        m_state->m_unknownOLEs.push_back(dOle.m_name);
    }

    if (!pict.isEmpty())
      m_state->m_idToObjectMap[id]=pict;
  }

  return true;
}



////////////////////////////////////////
//
// small structure
//
////////////////////////////////////////
bool OLEParser::readOle(librevenge::RVNGInputStream *ip, std::string const &oleName)
{
  if (!ip) return false;

  if (strcmp("Ole",oleName.c_str()) != 0) return false;
  return true;
}

bool OLEParser::readCompObj(librevenge::RVNGInputStream *ip, std::string const &oleName)
{
  if (!ip || strncmp(oleName.c_str(), "CompObj", 7) != 0) return false;

  // check minimal size
  const int minSize = 12 + 14+ 16 + 12; // size of header, clsid, footer, 3 string size
  if (ip->seek(minSize,librevenge::RVNG_SEEK_SET) != 0 || ip->tell() != minSize) return false;
  ip->seek(12, librevenge::RVNG_SEEK_SET);
  // the clsid
  unsigned long clsData[4]; // useme
  for (unsigned long &i : clsData)
    i = readU32(ip);
  return true;
}

//////////////////////////////////////////////////
//
// OlePres001 seems to contained standart picture file and size
//    extract the picture if it is possible
//
//////////////////////////////////////////////////

bool OLEParser::isOlePres(librevenge::RVNGInputStream *ip, std::string const &oleName)
{
  if (!ip || strncmp("OlePres",oleName.c_str(),7) != 0) return false;

  if (ip->seek(40, librevenge::RVNG_SEEK_SET) != 0 || ip->tell() != 40) return false;

  ip->seek(0, librevenge::RVNG_SEEK_SET);
  for (int i= 0; i < 2; i++)
  {
    long val = readS32(ip);
    if (val < -10 || val > 10) return false;
  }

  long actPos = ip->tell();
  int hSize = readS32(ip);
  if (hSize < 4) return false;
  if (ip->seek(actPos+hSize+28, librevenge::RVNG_SEEK_SET) != 0
      || ip->tell() != actPos+hSize+28)
    return false;

  ip->seek(actPos+hSize, librevenge::RVNG_SEEK_SET);
  for (int i= 3; i < 7; i++)
  {
    long val = readS32(ip);
    if (val < -10 || val > 10)
    {
      if (i != 5 || val > 256) return false;
    }
  }

  ip->seek(8, librevenge::RVNG_SEEK_CUR);
  long size = readS32(ip);

  if (size <= 0) return ip->isEnd();

  actPos = ip->tell();
  if (ip->seek(actPos+size, librevenge::RVNG_SEEK_SET) != 0
      || ip->tell() != actPos+size)
    return false;

  return true;
}

bool OLEParser::readOlePres(librevenge::RVNGInputStream *ip, EmbeddedObject &obj)
{
  if (!isOlePres(ip, "OlePres")) return false;

  ip->seek(8,librevenge::RVNG_SEEK_SET);

  long actPos = ip->tell();
  int hSize = readS32(ip);
  if (hSize < 4) return false;

  long endHPos = actPos+hSize;
  if (ip->seek(endHPos+28, librevenge::RVNG_SEEK_SET) != 0
      || ip->tell() != endHPos+28)
    return false;

  ip->seek(endHPos, librevenge::RVNG_SEEK_SET);

  actPos = ip->tell();
  ip->seek(24, librevenge::RVNG_SEEK_CUR); // flag and dim
  long fSize = readS32(ip);
  if (fSize == 0) return false;

  librevenge::RVNGBinaryData data;
  if (!readData(ip, static_cast<unsigned long>(fSize), data)) return false;
  obj.add(data);
  return true;
}

//////////////////////////////////////////////////
//
//  Ole10Native: basic Windows picture, with no size
//          - in general used to store a bitmap
//
//////////////////////////////////////////////////

bool OLEParser::isOle10Native(librevenge::RVNGInputStream *ip, std::string const &oleName)
{
  if (strncmp("Ole10Native",oleName.c_str(),11) != 0) return false;

  if (ip->seek(4, librevenge::RVNG_SEEK_SET) != 0 || ip->tell() != 4) return false;

  ip->seek(0, librevenge::RVNG_SEEK_SET);
  long size = readS32(ip);

  if (size <= 0) return false;
  if (ip->seek(4+size, librevenge::RVNG_SEEK_SET) != 0 || ip->tell() != 4+size)
    return false;

  return true;
}

bool OLEParser::readOle10Native(librevenge::RVNGInputStream *ip, EmbeddedObject &obj)
{
  if (!isOle10Native(ip, "Ole10Native")) return false;

  ip->seek(0,librevenge::RVNG_SEEK_SET);
  long fSize = readS32(ip);

  librevenge::RVNGBinaryData data;
  if (!readData(ip, static_cast<unsigned long>(fSize), data)) return false;
  obj.add(data);

  return true;
}

////////////////////////////////////////////////////////////////
//
// In general a picture : a PNG, an JPEG, a basic metafile,
//    find also a MSDraw.1.01 picture (with first bytes 0x78563412="xV4") or WordArt,
//    ( with first bytes "WordArt" )  which are not sucefull read
//    (can probably contain a list of data, but do not know how to
//     detect that)
//
// To check: does this is related to MSO_BLIPTYPE ?
//        or OO/filter/sources/msfilter/msdffimp.cxx ?
//
////////////////////////////////////////////////////////////////
bool OLEParser::readContents(librevenge::RVNGInputStream *input,
                             std::string const &oleName,
                             EmbeddedObject &obj)
{
  if (strcmp(oleName.c_str(),"Contents") != 0) return false;

  input->seek(0, librevenge::RVNG_SEEK_SET);

  // bdbox 0 : size in the file ?
  int dim = readS32(input);
  if (dim==0x12345678)
  {
    MSPUB_DEBUG_MSG(("OLEParser: warning: find a MSDraw picture, ignored\n"));
    return false;
  }
  input->seek(28, librevenge::RVNG_SEEK_CUR);
  if (input->isEnd())
  {
    MSPUB_DEBUG_MSG(("OLEParser: warning: Contents header length\n"));
    return false;
  }
  long actPos = input->tell();
  auto size = long(readU32(input));
  if (size <= 0 || input->seek(actPos+size+4, librevenge::RVNG_SEEK_SET)!=0 || !input->isEnd())
  {
    MSPUB_DEBUG_MSG(("OLEParser: warning: Contents unexpected file size=%ld\n",
                     size));
    return false;
  }
  input->seek(actPos+4, librevenge::RVNG_SEEK_SET);

  librevenge::RVNGBinaryData data;
  if (!readData(input,static_cast<unsigned long>(size), data)) return false;
  obj.add(data);
  return true;
}

////////////////////////////////////////////////////////////////
//
// Another different type of contents (this time in majuscule)
// we seem to contain the header of a EMF and then the EMF file
//
////////////////////////////////////////////////////////////////
bool OLEParser::readCONTENTS(librevenge::RVNGInputStream *input,
                             std::string const &oleName,
                             EmbeddedObject &obj)
{
  if (!input || strcmp(oleName.c_str(),"CONTENTS") != 0) return false;

  input->seek(0, librevenge::RVNG_SEEK_SET);

  auto hSize = long(readU32(input));
  if (input->isEnd()) return false;
  if (hSize <= 52 || input->seek(hSize+8,librevenge::RVNG_SEEK_SET) != 0
      || input->tell() != hSize+8)
  {
    MSPUB_DEBUG_MSG(("OLEParser: warning: CONTENTS headerSize=%ld\n",
                     hSize));
    return false;
  }

  // minimal checking of the "copied" header
  input->seek(4, librevenge::RVNG_SEEK_SET);
  auto type = long(readU32(input));
  if (type < 0 || type > 4) return false;
  auto newSize = long(readU32(input));

  if (newSize < 8) return false;
  input->seek(32+4+4, librevenge::RVNG_SEEK_CUR); // two bdbox+2:type+2:id
  auto dataLength = long(readU32(input));
  if (dataLength <= 0 || input->seek(hSize+4+dataLength,librevenge::RVNG_SEEK_SET) != 0
      || input->tell() != hSize+4+dataLength || !input->isEnd())
  {
    MSPUB_DEBUG_MSG(("OLEParser: warning: CONTENTS unexpected file length=%ld\n",
                     dataLength));
    return false;
  }

  input->seek(4+hSize, librevenge::RVNG_SEEK_SET);
  librevenge::RVNGBinaryData data;
  if (!readData(input, static_cast<unsigned long>(dataLength), data)) return false;
  obj.add(data);

  return true;
}
}

/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
