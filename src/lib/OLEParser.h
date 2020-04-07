/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MSPUB_OLE_PARSER_H
#define MSPUB_OLE_PARSER_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <librevenge-stream/librevenge-stream.h>

namespace libmspub
{
struct EmbeddedObject;
namespace OLEParserInternal
{
struct State;
}
/** \brief a class used to parse some basic oles
    Tries to read the different ole parts and stores their contents in form of picture.
 */
class OLEParser
{
public:
  /** constructor */
  OLEParser(std::function<int(std::string const &)> const &dirToIdFunc=getIdFromDirectory);

  /** destructor */
  ~OLEParser();

  /** tries to parse basic OLE (excepted mainName)
      \return false if fileInput is not an Ole file */
  bool parse(librevenge::RVNGInputStream *fileInput);

  //! returns the list of unknown ole
  std::vector<std::string> const &getNotParse() const;
  //! returns the list of data positions which have been read
  std::map<int,EmbeddedObject> const &getObjectsMap() const;

protected:
  //!  the "Ole" small structure : unknown contain
  static bool readOle(librevenge::RVNGInputStream *ip, std::string const &oleName);
  //!  the "CompObj" contains : UserType,ClipName,ProgIdName
  bool readCompObj(librevenge::RVNGInputStream *ip, std::string const &oleName);

  /** the OlePres001 seems to contain standart picture file and size */
  static bool isOlePres(librevenge::RVNGInputStream *ip, std::string const &oleName);
  /** extracts the picture of OlePres001 if it is possible */
  static bool readOlePres(librevenge::RVNGInputStream *ip, EmbeddedObject &obj);

  //! theOle10Native : basic Windows' picture, with no size
  static bool isOle10Native(librevenge::RVNGInputStream *ip, std::string const &oleName);
  /** extracts the picture if it is possible */
  static bool readOle10Native(librevenge::RVNGInputStream *ip, EmbeddedObject &obj);

  /** \brief the Contents : in general a picture : a PNG, an JPEG, a basic metafile,
   * I find also a Word art picture, which are not sucefully read
   */
  static bool readContents(librevenge::RVNGInputStream *input, std::string const &oleName,
                           EmbeddedObject &obj);

  /** the CONTENTS : seems to store a header size, the header
   * and then a object in EMF (with the same header)...
   * \note I only find such lib in 2 files, so the parsing may be incomplete
   *  and many such Ole rejected
   */
  static bool readCONTENTS(librevenge::RVNGInputStream *input, std::string const &oleName,
                           EmbeddedObject &obj);

  /** the default function which uses the last integer of dirName to return the final id,
    ie. it converts "MatOST/MatadorObject1", "Object 1" in 1 */
  static int getIdFromDirectory(std::string const &dirName);

  //! the main state
  std::shared_ptr<OLEParserInternal::State> m_state;
};
}

#endif
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
