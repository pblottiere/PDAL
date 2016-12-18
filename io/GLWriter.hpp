// GLWriter.hpp

#pragma once

#include <pdal/Writer.hpp>
#include <pdal/Filter.hpp>
#include <pdal/plugin.hpp>

#include <string>

extern "C" int32_t GLWriter_ExitFunc();
extern "C" PF_ExitFunc GLWriter_InitPlugin();

extern "C" int32_t GLFilter_ExitFunc();
extern "C" PF_ExitFunc GLFilter_InitPlugin();

namespace pdal{

  class GLHelper;

  class GLWriter : public Writer
  {
  public:
    GLWriter()
    {}

    static void  * create();
    static int32_t destroy(void *);
    std::string getName() const;

  private:
    virtual void addArgs(ProgramArgs& args);
    virtual void initialize();
    virtual void ready(PointTableRef table);
    virtual void write(const PointViewPtr view);
    virtual void done(PointTableRef table);


    GLHelper* helper;
    bool m_one_color_per_batch;
    bool m_merge_views;
    std::string m_filename;
  };

  class PDAL_DLL GLFilter : public Filter
  {
  public:
    GLFilter() : Filter()
    {}

    static void * create();
    static int32_t destroy(void *);
    std::string getName() const;

    virtual void initialize();

  private:
    virtual void addArgs(ProgramArgs& args);
    virtual PointViewSet run(PointViewPtr view);
    virtual void done(PointTableRef table);

    GLHelper* helper;
    bool m_one_color_per_batch;
    bool m_merge_views;

    GLFilter& operator=(const GLFilter&); // not implemented
    GLFilter(const GLFilter&); // not implemented
  };

} // namespace pdal
