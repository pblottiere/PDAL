/******************************************************************************
* Copyright (c) 2016, Paul Blottiere (paul.blottiere@oslandia.com)
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following
* conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
*       names of its contributors may be used to endorse or promote
*       products derived from this software without specific prior
*       written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
* OF SUCH DAMAGE.
****************************************************************************/

#include <pdal/pdal_internal.hpp>

#include "RevertMortonFilter.hpp"
#include <pdal/StageFactory.hpp>
#include <pdal/pdal_macros.hpp>

namespace pdal
{

static PluginInfo const s_info = PluginInfo(
    "filters.revertmorton",
    "Filter points according to the revert Morton algorithm",
    "http://pdal.io/stages/filters.revertmorton.html" );

CREATE_SHARED_PLUGIN(1, 0, RevertMortonFilter, Filter, s_info)

uint32_t part1_by1(uint32_t x)
{
    x &= 0x0000ffff;
    x = (x ^ (x <<  8)) & 0x00ff00ff;
    x = (x ^ (x <<  4)) & 0x0f0f0f0f;
    x = (x ^ (x <<  2)) & 0x33333333;
    x = (x ^ (x <<  1)) & 0x55555555;
    return x;
}

uint32_t part1_by2(uint32_t x)
{
    x &= 0x000003ff;
    x = (x ^ (x << 16)) & 0xff0000ff;
    x = (x ^ (x <<  8)) & 0x0300f00f;
    x = (x ^ (x <<  4)) & 0x030c30c3;
    x = (x ^ (x <<  2)) & 0x09249249;
    return x;
}

uint32_t encode_morton(uint32_t x, uint32_t y)
{
    return (part1_by1(y) << 1) + part1_by1(x);
}

uint32_t revert_morton(uint32_t index)
{
    index = ((index >> 1) & 0x55555555u) | ((index & 0x55555555u) << 1);
    index = ((index >> 2) & 0x33333333u) | ((index & 0x33333333u) << 2);
    index = ((index >> 4) & 0x0f0f0f0fu) | ((index & 0x0f0f0f0fu) << 4);
    index = ((index >> 8) & 0x00ff00ffu) | ((index & 0x00ff00ffu) << 8);
    index = ((index >> 16) & 0xffffu) | ((index & 0xffffu) << 16);
    return index;
}

std::string RevertMortonFilter::getName() const { return s_info.name; }

Options RevertMortonFilter::getDefaultOptions()
{
    Options options;
    return options;
}


void RevertMortonFilter::processOptions(const Options& options)
{
}


void RevertMortonFilter::ready(PointTableRef table)
{
}

PointViewSet RevertMortonFilter::run(PointViewPtr view)
{
    int32_t cell = sqrt(view->size());

    // compute range
    BOX2D buffer_bounds;
    view->calculateBounds(buffer_bounds);
    double xrange = buffer_bounds.maxx - buffer_bounds.minx;
    double yrange = buffer_bounds.maxy - buffer_bounds.miny;

    double cell_width = xrange / cell;
    double cell_height = yrange / cell;

    // compute revert morton code for each point

    std::map<uint32_t, PointId> codes;
    for (PointId idx = 0; idx < view->size(); idx++)
    {
        int32_t xpos = floor( (view->getFieldAs<double>(Dimension::Id::X, idx) -
            buffer_bounds.minx) / cell_width );
        int32_t ypos = floor( (view->getFieldAs<double>(Dimension::Id::Y, idx) -
            buffer_bounds.miny) / cell_height ) ;

        uint32_t code = revert_morton( encode_morton(xpos, ypos) );
        codes[code] = idx;
    }

    // a map is yet order by key so its naturally ordered by lod
    std::map<uint32_t, PointId>::iterator it;
    PointViewPtr outview = view->makeNew();
    for( it = codes.begin(); it != codes.end(); ++it)
        outview->appendPoint(*view, it->second);

    // build output view
    PointViewSet viewSet;
    viewSet.insert(outview);

    return viewSet;
}

void RevertMortonFilter::done(PointTableRef table)
{
}

}
