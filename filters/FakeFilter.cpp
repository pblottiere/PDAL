/******************************************************************************
* Copyright (c) 2017, Hobu Inc., info@hobu.co
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

#include "FakeFilter.hpp"

#include <pdal/PipelineManager.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/util/ProgramArgs.hpp>

#include "private/DimRange.hpp"

#include <iostream>
#include <utility>
#include <chrono>

using namespace std::chrono;

namespace pdal
{

static PluginInfo const s_info = PluginInfo(
    "filters.fake",
    "Re-assign some point attributes based KNN voting",
    "http://pdal.io/stages/filters.neighborclassifier.html" );

CREATE_STATIC_STAGE(FakeFilter, s_info)

FakeFilter::FakeFilter() :
    m_dim(Dimension::Id::Classification)
{}


FakeFilter::~FakeFilter()
{}


void FakeFilter::addArgs(ProgramArgs& args)
{
    args.add("k", "Number of nearest neighbors to consult",
        m_k).setPositional();
    args.add("candidate", "candidate file name", m_candidateFile);
}

void FakeFilter::initialize()
{
    std::cout << "FakeFilter::initialize" << std::endl;
    for (auto const& r : m_domainSpec)
    {
        try
        {
            DimRange range;
            range.parse(r);
            m_domain.push_back(range);
        }
        catch (const DimRange::error& err)
        {
            throwError("Invalid 'domain' option: '" + r + "': " + err.what());
        }
    }
    if (m_k < 1)
        throwError("Invalid 'k' option: " + std::to_string(m_k) +
            ", must be > 0");
}


void FakeFilter::prepared(PointTableRef table)
{
    std::cout << "FakeFilter::prepared 0" << std::endl;
    PointLayoutPtr layout(table.layout());

    for (auto& r : m_domain)
    {
        std::cout << "FakeFilter::prepared 1" << std::endl;
        r.m_id = layout->findDim(r.m_name);
        if (r.m_id == Dimension::Id::Unknown)
            throwError("Invalid dimension name in 'domain' option: '" +
                r.m_name + "'.");
    }
    std::cout << "FakeFilter::prepared 2" << std::endl;
    std::sort(m_domain.begin(), m_domain.end());
    std::cout << "FakeFilter::prepared 3" << std::endl;
}

void FakeFilter::doOneNoDomain(PointRef &point, PointRef &temp,
    KD3Index &kdi)
{
    std::cout << "FakeFilter::doOneNoDomain" << std::endl;
    PointIdList iSrc = kdi.neighbors(point, m_k);
    double thresh = iSrc.size()/2.0;

    // vote NNs
    using CountMap = std::map<int, unsigned int>;
    CountMap counts;
    //std::map<int, unsigned int> counts;
    for (PointId id : iSrc)
    {
        temp.setPointId(id);
        counts[temp.getFieldAs<int>(m_dim)]++;
    }

    // pick winner of the vote
    auto pr = *std::max_element(counts.begin(), counts.end(),
        [](CountMap::const_reference p1, CountMap::const_reference p2)
        { return p1.second < p2.second; });

    // update point
    auto oldclass = point.getFieldAs<double>(m_dim);
    auto newclass = pr.first;
    if (pr.second > thresh && oldclass != newclass)
    {
        point.setField(m_dim, newclass);
    }
}

// update point.  kdi and temp both reference the NN point cloud
bool FakeFilter::doOne(PointRef& point, PointRef &temp,
    KD3Index &kdi)
{
    std::cout << "FakeFilter::doOne" << std::endl;
    if (m_domain.empty())  // No domain, process all points
        doOneNoDomain(point, temp, kdi);

    for (DimRange& r : m_domain)
    {   // process only points that satisfy a domain condition
        if (r.valuePasses(point.getFieldAs<double>(r.m_id)))
        {
            doOneNoDomain(point, temp, kdi);
            break;
        }
    }
    return true;
}


PointViewPtr FakeFilter::loadSet(const std::string& filename,
    PointTable& table)
{
    std::cout << "FakeFilter::loadSet" << std::endl;
    PipelineManager mgr;

    Stage& reader = mgr.makeReader(filename, "");
    reader.prepare(table);
    PointViewSet viewSet = reader.execute(table);
    assert(viewSet.size() == 1);
    return *viewSet.begin();
}

void FakeFilter::filter(PointView& view)
{
    PointRef point_src(view, 0);

    auto start = high_resolution_clock::now();
    KD3Index& kdiSrc = view.build3dIndex();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    std::cout << "kdtree: " << duration.count() / 1000000 << std::endl;
    PointRef point_nn(view, 0);

    start = high_resolution_clock::now();
    for (PointId id = 0; id < view.size(); ++id)
    {
        point_src.setPointId(id);
        // PointIdList iSrc = kdiSrc.neighbors(point_src, m_k);
        PointIdList iSrc = kdiSrc.radius(point_src, m_k);
    }
    stop = high_resolution_clock::now();
    duration = duration_cast<microseconds>(stop - start);
    std::cout << "fake: " << duration.count() / 1000000 << std::endl;
}

} // namespace pdal
