/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * This file is part of the libmspub project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_BORDERARTINFO_H
#define INCLUDED_BORDERARTINFO_H

#include <vector>

#include <librevenge/librevenge.h>

#include "MSPUBTypes.h"

namespace libmspub
{
struct BorderImgInfo
{
  ImgType m_type;
  librevenge::RVNGBinaryData m_imgBlob;
  BorderImgInfo(ImgType type) :
    m_type(type), m_imgBlob()
  {
  }
};

struct BorderArtInfo
{
  std::vector<BorderImgInfo> m_images;
  std::vector<unsigned> m_offsets;
  BorderArtInfo()
    : m_images()
    , m_offsets()
  {
  }

};
}
#endif
/* vim:set shiftwidth=2 softtabstop=2 expandtab: */
