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

#include "MidocFilter.hpp"
#include <pdal/StageFactory.hpp>
#include <pdal/pdal_macros.hpp>

namespace pdal
{

static PluginInfo const s_info = PluginInfo(
    "filters.midoc",
    "Filter points according to the midoc algorithm",
    "http://pdal.io/stages/filters.midoc.html" );

CREATE_SHARED_PLUGIN(1, 0, MidocFilter, Filter, s_info)

std::string MidocFilter::getName() const { return s_info.name; }

Options MidocFilter::getDefaultOptions()
{
    Options options;
    return options;
}


void MidocFilter::processOptions(const Options& options)
{
}


void MidocFilter::ready(PointTableRef table)
{
}

void MidocFilter::split(const BOX2D &box, std::vector<BOX2D> &boxes)
{
    double width = box.maxx - box.minx;
    double height = box.maxy - box.miny;

    int n = 2;
    double stepx = width / n;
    double stepy = height / n;

    for ( int i = 0; i < n; i++)
    {
        for ( int j = 0; j < n; j++)
        {
            double xbeg = box.minx + i*stepx;
            double ybeg = box.miny + j*stepy;
            BOX2D bb(xbeg, ybeg, xbeg + stepx, ybeg + stepy);
            boxes.push_back(bb);
        }
    }
}

void MidocFilter::midoc(const BOX2D &box,
        int level,
        PointViewPtr view,
        std::vector<PointId> &dataset,
        std::multimap<int,PointId> &sorted)
{
    int index = -1;
    double mindist = std::numeric_limits<double>::max();
    double centerx = box.minx + (box.maxx - box.minx)/2;
    double centery = box.miny + (box.maxy - box.miny)/2;

    std::vector<BOX2D> boxes;
    split(box, boxes);
    std::vector<std::vector<PointId>> split_dataset;
    for ( size_t i = 0; i < boxes.size(); i++ )
    {
        std::vector<PointId> data;
        split_dataset.push_back(data);
    }

    bool found = false;
    int boxnumber = -1;
    int boxindex = -1;
    for ( size_t j = 0; j < dataset.size(); j++)
    {
        found = false;
        PointId idx = dataset[j];
        double x = view->getFieldAs<double>(Dimension::Id::X, idx);
        double y = view->getFieldAs<double>(Dimension::Id::Y, idx);

        if (box.contains(x, y))
        {
            double t1 = std::pow(centerx - x, 2);
            double t2 = std::pow(centery - y, 2);
            double dist = std::sqrt(t1 + t2);

            if ( dist < mindist )
            {
                mindist = dist;
                index = j;
                found = true;
            }
        }

        for ( size_t i = 0; i < boxes.size(); i++ )
        {
            if (boxes[i].contains(x,y))
            {
                split_dataset[i].push_back(idx);

                if (found)
                {
                    boxnumber = i;
                    boxindex = split_dataset[i].size()-1;
                }

                break;
            }
        }
    }

    if ( index  != -1 )
    {
        sorted.insert( std::pair<int, PointId>(level, dataset[index]) );
        dataset.erase(dataset.begin()+index);
    }

    if ( boxindex != -1 )
    {
        split_dataset[boxnumber].erase(split_dataset[boxnumber].begin()+boxindex);
    }

    if (dataset.size() > 0 && index != -1)
    {
        int next_level = level+1;

        for ( size_t i = 0; i < boxes.size(); i++ )
            midoc(boxes[i], next_level, view, split_dataset[i], sorted);
    }
}


PointViewSet MidocFilter::run(PointViewPtr view)
{
    PointViewPtr outview = view->makeNew();

    std::vector<PointId> dataset;
    for (PointId idx = 0; idx < view->size(); idx++)
        dataset.push_back( idx );
    std::multimap<int,PointId> sorted;

    BOX2D fullbb;
    view->calculateBounds(fullbb);
    midoc(fullbb, 0, view, dataset, sorted);

    // a map is yet order by key so its naturally ordered by lod
    std::multimap<int, PointId>::iterator it;
    for( it = sorted.begin(); it != sorted.end(); ++it)
        outview->appendPoint(*view, it->second);

    PointViewSet viewSet;
    viewSet.insert(outview);

    return viewSet;
}


void MidocFilter::done(PointTableRef table)
{
}

}
