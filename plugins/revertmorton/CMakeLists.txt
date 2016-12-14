
# Midoc Filter
#
set(srcs filters/RevertMortonFilter.cpp)
set(incs filters/RevertMortonFilter.hpp)

PDAL_ADD_PLUGIN(revertmorton_libname filter revertmorton
    FILES "${srcs}" "${incs}")
