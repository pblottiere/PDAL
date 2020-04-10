#pragma once

#include <pdal/Filter.hpp>
#include <pdal/KDIndex.hpp>

extern "C" int32_t FakeFilter_ExitFunc();
extern "C" PF_ExitFunc FakeFilter_InitPlugin();

namespace pdal
{

struct DimRange;

class PDAL_DLL FakeFilter : public Filter
{
public:
    FakeFilter();
    ~FakeFilter();

    static void * create();
    static int32_t destroy(void *);
    std::string getName() const { return "filters.neighborclassifier"; }

private:
    virtual void addArgs(ProgramArgs& args);
    virtual void prepared(PointTableRef table);
    bool doOne(PointRef& point, PointRef& temp, KD3Index &kdi);
    virtual void filter(PointView& view);
    virtual void initialize();
    void doOneNoDomain(PointRef &point, PointRef& temp, KD3Index &kdi);
    PointViewPtr loadSet(const std::string &candFileName, PointTable &table);
    FakeFilter& operator=(const FakeFilter&) = delete;
    FakeFilter(const FakeFilter&) = delete;
    StringList m_domainSpec;
    std::vector<DimRange> m_domain;
    int m_k;
    Dimension::Id m_dim;
    std::string m_dimName;
    std::string m_candidateFile;
};

} // namespace pdal
