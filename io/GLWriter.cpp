// GLWriter.cpp

#include "GLWriter.hpp"
#include <pdal/pdal_macros.hpp>
#include <pdal/util/ProgramArgs.hpp>
#include <cfloat>

extern "C" {
void* init_graphics();
void render_one_frame(void*, int);
void render_loop(void*);
void push_points(void*, float* xyz, uint8_t* rgbi, size_t count, float* minmax);
}

namespace pdal
{

class GLHelper {
  public:
    GLHelper() {
        gl_ctx = init_graphics();

        // gen palettes
        float a[] = { 0.5, 0.5, 0.5 };
        float b[] = { 0.5, 0.5, 0.5 };
        float c[] = { 1.0, 1.0, 0.5 };
        float d[] = { 0.8, 0.9, 0.3 };

        float two_pi = 3.14 * 2;
        for (int i = 0; i < 100; i++) {
            for (int j = 0; j < 3; j++) {
                palettes.push_back(
                    255 *
                    (a[j] + b[j] * cos(two_pi * (c[j] * i / 100.0 + d[j]))));
            }
            palettes.push_back(255);
        }

        minmax[0] = minmax[2] = minmax[4] =  FLT_MAX;
        minmax[1] = minmax[3] = minmax[5] = -FLT_MAX;

        xyz = NULL;
        buffer_elements = 0;
        rgbi = NULL;

        previousColor = 0;
    }

    void extractPoints(PointViewPtr view, bool oneColorPerBatch) {
        uint8_t* colorOverride = NULL;
        if (oneColorPerBatch) {
            int s = palettes.size() / 4;
            previousColor = (previousColor + s - 3) % s;
            colorOverride = &palettes.data()[4 * previousColor];
        }

        xyz = (float*)realloc(xyz,
                              3 * sizeof(float) * (buffer_elements + view->size()));
        rgbi = (uint8_t*)realloc(rgbi, 4 * sizeof(uint8_t) *
                                           (buffer_elements + view->size()));

        extractPoints(gl_ctx, view, &xyz[3 * buffer_elements],
                   &rgbi[4 * buffer_elements], minmax, colorOverride);

        buffer_elements += view->size();
    }


    void flushPoints() {
        push_points(gl_ctx, xyz, rgbi, buffer_elements, minmax);

        free(xyz);
        free(rgbi);
    }

    void renderLoop() {
        render_loop(gl_ctx);
    }

  private:

    static void extractPoints(void* ctx, PointViewPtr view, float* xyz, uint8_t* rgbi,
                    float* minmax, uint8_t* colorOverride) {

        Dimension::Id dims[] = { Dimension::Id::X,        Dimension::Id::Z,
                                 Dimension::Id::Y,        Dimension::Id::Red,
                                 Dimension::Id::Green,    Dimension::Id::Blue,
                                 Dimension::Id::Intensity };

        float scales[4];
        for (int i = 0; i < 4; i++) {
            switch (view->dimType(dims[i + 3])) {
                case Dimension::Type::Unsigned16:
                    scales[i] = 1.0;//255.0 / 65535.0;
                    break;
                default:
                    scales[i] = 1.0;
            }
        }

        // float* _xyz = &xyz[3 * buffer_elements];
        for (PointId idx = 0; idx < view->size(); ++idx) {
            for (int i = 0; i < 3; i++) {
                float s = xyz[3 * idx + i] =
                    view->getFieldAs<double>(dims[i], idx);
                minmax[2*i] = std::min(minmax[2*i], s);
                minmax[2*i+1] = std::max(minmax[2*i+1], s);
            }

            // uint8_t* _rgbi = &rgbi[4 * buffer_elements];
            if (colorOverride) {
                memcpy(&rgbi[4 * idx], colorOverride, 4);
            } else {
                for (int i = 0; i < 4; i++) {
                    if (view->hasDim(dims[i + 3])) {
                        rgbi[4 * idx + i] =
                            view->getFieldAs<float>(dims[i + 3], idx) *
                            scales[i];
                    } else {
                        rgbi[4 * idx + i] = 255;
                    }
                }
            }
        }
    }

    void* gl_ctx;
    float minmax[6];

    float* xyz;
    int64_t buffer_elements;
    uint8_t* rgbi;

    std::vector<uint8_t> palettes;
    int previousColor;
};

}

namespace pdal
{
static PluginInfo const s_info = PluginInfo("writers.gl", "My OpenGL Writer",
                                            "http://path/to/documentation");

CREATE_STATIC_PLUGIN(1, 0, GLWriter, Writer, s_info);

std::string GLWriter::getName() const { return s_info.name; }

void GLWriter::addArgs(ProgramArgs& args) {
    args.add("filename", "Output filename", m_filename).setPositional();
    args.add("color_batch", "Use one color per batch", m_one_color_per_batch, false);
    args.add("merge_views", "Merge all views in 1 GPU obj", m_merge_views, true);
}

void GLWriter::initialize() { helper = new GLHelper(); }

void GLWriter::ready(PointTableRef table) {}

void GLWriter::write(PointViewPtr view) {
    helper->extractPoints(view, m_one_color_per_batch);
    if (!m_merge_views) {
        helper->flushPoints();
    }
}

void GLWriter::done(PointTableRef) {
    if (m_merge_views) {
        helper->flushPoints();
    }
    helper->renderLoop();

    delete helper;
}

static PluginInfo const s_info2 =
    PluginInfo("filters.gl", "My GL filter", "http://link/to/documentation");

CREATE_STATIC_PLUGIN(1, 0, GLFilter, Filter, s_info2)

std::string GLFilter::getName() const { return s_info.name; }

void GLFilter::addArgs(ProgramArgs& args) {
    args.add("color_batch", "Use one color per batch", m_one_color_per_batch, false);
    args.add("merge_views", "Merge all views in 1 GPU obj", m_merge_views, true);
}

void GLFilter::initialize() {
    helper = new GLHelper();
}

PointViewSet GLFilter::run(PointViewPtr view) {
    helper->extractPoints(view, m_one_color_per_batch);
    if (!m_merge_views) {
        helper->flushPoints();
    }

    PointViewSet viewSet;
    viewSet.insert(view);

    return viewSet;
}

void GLFilter::done(PointTableRef) {
    if (m_merge_views) {
        helper->flushPoints();
    }
    helper->renderLoop();

    delete helper;
}

} // namespace pdal
