
# Midoc Filter
#
set(srcs filters/MidocFilter.cpp)
set(incs filters/MidocFilter.hpp)

PDAL_ADD_PLUGIN(midoc_libname filter midoc
    FILES "${srcs}" "${incs}")
