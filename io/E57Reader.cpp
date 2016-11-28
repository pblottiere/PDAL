/******************************************************************************
* Copyright (c) 2015, Peter J. Gadomski <pete.gadomski@gmail.com>
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

#include "E57Reader.hpp"

#include <E57Foundation.h>
#include <E57Simple.h>
#include <sstream>

#include <pdal/PointView.hpp>
#include <pdal/pdal_macros.hpp>

namespace pdal
{

static PluginInfo const s_info = PluginInfo(
        "readers.e57",
        "Read E57 files.",
        "http://pdal.io/stages/reader.e57.html");


CREATE_STATIC_PLUGIN(1, 0, E57Reader, Reader, s_info);


std::string E57Reader::getName() const
{
    return s_info.name;
}


E57Reader::E57Reader()
    : m_scan_index(-1),
      m_batch_size(10000l),
      m_vertexDimensions()
{}


Dimension::IdList E57Reader::getDefaultDimensions()
{
Dimension::IdList ids;

ids.push_back(Dimension::Id::X);
ids.push_back(Dimension::Id::Y);
ids.push_back(Dimension::Id::Z);

return ids;
}

void E57Reader::addArgs(ProgramArgs& args)
{
    args.add("scan_index", "Scan index (default: -1 (all))", m_scan_index, -1);
    args.add("batch_size", "Batch read size (default: 10000)", m_batch_size, 10000);
}


void E57Reader::initialize()
{
    reader = new e57::Reader(m_filename);
}

#if 0
    p_ply ply = openPly(m_filename);
    p_ply_element vertex_element = nullptr;
    bool found_vertex_element = false;
    const char* element_name;
    long element_count;
    while ((vertex_element = ply_get_next_element(ply, vertex_element)))
    {
        if (!ply_get_element_info(vertex_element, &element_name, &element_count))
        {
            std::stringstream ss;
            ss << "Error reading element info in " << m_filename << ".";
            throw pdal_error(ss.str());
        }
        if (strcmp(element_name, "vertex") == 0)
        {
            found_vertex_element = true;
            break;
        }
    }
    if (!found_vertex_element)
    {
        std::stringstream ss;
        ss << "File " << m_filename << " does not contain a vertex element.";
        throw pdal_error(ss.str());
    }

    static std::map<int, Dimension::Type> types =
    {
        { PLY_INT8, Dimension::Type::Signed8 },
        { PLY_UINT8, Dimension::Type::Unsigned8 },
        { PLY_INT16, Dimension::Type::Signed16 },
        { PLY_UINT16, Dimension::Type::Unsigned16 },
        { PLY_INT32, Dimension::Type::Signed32 },
        { PLY_UINT32, Dimension::Type::Unsigned32 },
        { PLY_FLOAT32, Dimension::Type::Float },
        { PLY_FLOAT64, Dimension::Type::Double },

        { PLY_CHAR, Dimension::Type::Signed8 },
        { PLY_UCHAR, Dimension::Type::Unsigned8 },
        { PLY_SHORT, Dimension::Type::Signed16 },
        { PLY_USHORT, Dimension::Type::Unsigned16 },
        { PLY_INT, Dimension::Type::Signed32 },
        { PLY_UINT, Dimension::Type::Unsigned32 },
        { PLY_FLOAT, Dimension::Type::Float },
        { PLY_DOUBLE, Dimension::Type::Double }
    };

    p_ply_property property = nullptr;
    while ((property = ply_get_next_property(vertex_element, property)))
    {
        const char* name;
        e_ply_type type;
        e_ply_type length_type;
        e_ply_type value_type;
        if (!ply_get_property_info(property, &name, &type,
            &length_type, &value_type))
        {
            std::stringstream ss;
            ss << "Error reading property info in " << m_filename << ".";
            throw pdal_error(ss.str());
        }
        // For now, we'll just use PDAL's built in dimension matching.
        // We could be smarter about this, e.g. by using the length
        // and value type attributes.
        m_vertexTypes[name] = types[type];
    }
    ply_close(ply);
}
#endif


void E57Reader::addDimensions(PointLayoutPtr layout)
{
    // default dimensions
    layout->registerDim(Dimension::Id::X);
    layout->registerDim(Dimension::Id::Y);
    layout->registerDim(Dimension::Id::Z);

    uint32_t data3dCount = reader->GetData3DCount();
    e57::Data3D header;
    for (uint32_t scanIndex=0; scanIndex<data3dCount; scanIndex++) {
        if (reader->ReadData3D(scanIndex, header)) {
            if (header.pointFields.colorRedField) {
                layout->registerDim(Dimension::Id::Red);
            }
            if (header.pointFields.colorGreenField) {
                layout->registerDim(Dimension::Id::Green);
            }
            if (header.pointFields.colorBlueField) {
                layout->registerDim(Dimension::Id::Blue);
            }
            if (header.pointFields.intensityField) {
                layout->registerDim(Dimension::Id::Intensity);
            }
        } else {
            std::stringstream ss;
            ss << "e57: can't read Data3D header " << scanIndex << '/' << data3dCount;
            throw pdal_error(ss.str());
        }
        break; // TODO
    }
}

QuickInfo E57Reader::inspect()
{
    QuickInfo qi;
    std::unique_ptr<PointLayout> layout(new PointLayout());

    PointTable table;
    initialize();
    addDimensions(layout.get());

    Dimension::IdList dims = layout->dims();
    for (auto di = dims.begin(); di != dims.end(); ++di) {
        qi.m_dimNames.push_back(layout->dimName(*di));
    }

    uint32_t data3dCount = reader->GetData3DCount();
    qi.m_pointCount = 0;
    e57::Data3D header;

    uint32_t scanIndex = 0;
    if (0 <= m_scan_index) {
        if (data3dCount <= m_scan_index) {
            std::stringstream ss;
            ss << "e57: invalid scanIndex " << m_scan_index  << ". Only " <<  data3dCount << " scans in file";
            throw pdal_error(ss.str());
        }
        scanIndex = m_scan_index;
        data3dCount = m_scan_index + 1;
    }

    for (; scanIndex<data3dCount; scanIndex++) {
        int64_t nColumn = 0;
        int64_t nRow = 0;
        int64_t nPointsSize = 0;    //Number of points
        int64_t nGroupsSize = 0;    //Number of groups
        int64_t nCountSize = 0;     //Number of points per group
        bool bColumnIndex;
        if (reader->GetData3DSizes( scanIndex, nRow, nColumn, nPointsSize, nGroupsSize, nCountSize, bColumnIndex)) {
            qi.m_pointCount += nPointsSize;
        }

        e57::Data3D     scanHeader;
        reader->ReadData3D( scanIndex, scanHeader);

        qi.m_bounds.grow(
            scanHeader.cartesianBounds.xMinimum,
            scanHeader.cartesianBounds.yMinimum,
            scanHeader.cartesianBounds.zMinimum);
        qi.m_bounds.grow(
            scanHeader.cartesianBounds.xMaximum,
            scanHeader.cartesianBounds.yMaximum,
            scanHeader.cartesianBounds.zMaximum);
    }

    qi.m_valid = true;

    done(table);

    return qi;
}

point_count_t E57Reader::read(PointViewPtr view, point_count_t num)
{
    uint32_t data3dCount = reader->GetData3DCount();
    point_count_t numRead = 0;

    std::vector<double> cartesianX, cartesianY, cartesianZ;
    std::vector<uint16_t> colorRed, colorGreen, colorBlue;
    std::vector<double> intensity;

    uint32_t scanIndex = 0;
    if (0 <= m_scan_index) {
        if (data3dCount <= m_scan_index) {
            std::stringstream ss;
            ss << "e57: invalid scanIndex " << m_scan_index << ". Only " <<  data3dCount << " scans in file";
            throw pdal_error(ss.str());
        }
        scanIndex = m_scan_index;
        data3dCount = m_scan_index + 1;
    }


    float sample_rate = 1.0;
    uint64_t total_point_count = 0;
    for (uint32_t s = scanIndex; s<data3dCount; s++) {
        int64_t nColumn = 0;
        int64_t nRow = 0;
        int64_t nPointsSize = 0;    //Number of points
        int64_t nGroupsSize = 0;    //Number of groups
        int64_t nCountSize = 0;     //Number of points per group
        uint32_t columnMax, pointsSize, groupsSize, countSize;
        bool bColumnIndex;

        if (reader->GetData3DSizes( scanIndex, nRow, nColumn, nPointsSize, nGroupsSize, nCountSize, bColumnIndex)) {
            total_point_count += nPointsSize;
        }
    }
    if (num < total_point_count) {
        sample_rate = num / (float)total_point_count;
    }

    float sample = 1.0;
    for (; scanIndex<data3dCount && numRead < num; scanIndex++) {

        int64_t nColumn = 0;
        int64_t nRow = 0;
        int64_t nPointsSize = 0;    //Number of points
        int64_t nGroupsSize = 0;    //Number of groups
        int64_t nCountSize = 0;     //Number of points per group
        uint32_t columnMax, pointsSize, groupsSize, countSize;
        bool bColor, bIntensity;
        bool bColumnIndex;

        e57::Data3D     scanHeader;
        reader->ReadData3D( scanIndex, scanHeader);

        bColor = scanHeader.pointFields.colorRedField;
        bIntensity = scanHeader.pointFields.intensityField;
        double intensityScale = scanHeader.pointFields.intensityScaledInteger;
        if (intensityScale == 0) {
            intensityScale = 1.0;
        }

        if (reader->GetData3DSizes( scanIndex, nRow, nColumn, nPointsSize, nGroupsSize, nCountSize, bColumnIndex)) {
            if (nRow == 0) {
                nRow = std::min((int64_t)m_batch_size, nPointsSize);
            }

            if (cartesianX.size() < nRow) {
                cartesianX.resize(nRow);
                cartesianY.resize(nRow);
                cartesianZ.resize(nRow);
                colorRed.resize(nRow);
                colorGreen.resize(nRow);
                colorBlue.resize(nRow);
                intensity.resize(nRow);
            }
        }

        // std::cout << nRow << 'x' << nColumn << "=>" << nPointsSize << std::endl;

        e57::CompressedVectorReader dataReader = reader->SetUpData3DPointsData(scanIndex,
            nRow,
            cartesianX.data(),
            cartesianY.data(),
            cartesianZ.data(),
            NULL,
            intensity.data(),
            NULL,
            colorRed.data(),
            colorGreen.data(),
            colorBlue.data(),
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            );

        {
            PointId nextId = view->size();

            int64_t     count = 0;
            unsigned    size = 0;

            while((size = dataReader.read()) && (numRead < num))
            {
                // std::cout << "size:" << size << std::endl;
                //Use the data
                for(long i = 0; i < size && numRead < num; i++)
                {
                    sample += sample_rate;

                    if (sample >= 1) {
                        view->setField(Dimension::Id::X, nextId, cartesianX[i]);
                        view->setField(Dimension::Id::Y, nextId, cartesianY[i]);
                        view->setField(Dimension::Id::Z, nextId, cartesianZ[i]);

                        if(bColor){
                            view->setField(Dimension::Id::Red, nextId, colorRed[i]);
                            view->setField(Dimension::Id::Green, nextId, colorGreen[i]);
                            view->setField(Dimension::Id::Blue, nextId, colorBlue[i]);
                        }
                        if(bIntensity) {
                            view->setField(Dimension::Id::Intensity, nextId, intensity[i] / intensityScale);
                        }

                        // count++;
                        numRead++;
                        nextId++;

                        sample -= 1;
                    }

                }
                // std::cout << "-> numRead=" << numRead << std::endl;
            }
        }
        dataReader.close();
    }
    // std::cout << "Total points:" << numRead << std::endl;
    return numRead;
}


void E57Reader::done(PointTableRef table)
{
    if (!reader->Close())
    {
        std::stringstream ss;
        ss << "e57: error closing " << m_filename << ".";
        throw pdal_error(ss.str());
    }
    delete reader;
}

} // namespace pdal
