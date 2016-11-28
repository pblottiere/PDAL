#
# Make sure we don't attempt to add a library more than once
#

if (NOT PDAL_E57_ENABLED)
    return()
endif()

get_property(EXISTS GLOBAL PROPERTY _PDAL_E57_INCLUDED)
if (EXISTS)
    return()
endif()


SET(CMAKE_CXX_FLAGS "-std=c++11")


add_definitions("-fPIC")


INCLUDE(${CMAKE_MODULE_PATH}/FindXerces.cmake)
INCLUDE(${CMAKE_MODULE_PATH}/FindICU.cmake)

SET(Xerces_USE_SHARED_LIBS On)
FIND_PACKAGE(Xerces REQUIRED)
if(Xerces_FOUND)
	set(XML_LIBRARIES ${Xerces_LIBRARY})
	set(XML_INCLUDE_DIRS ${Xerces_INCLUDE_DIR})
	message(STATUS "Xerces Library (include: ${Xerces_LIBRARY})")
	if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	    add_definitions(-DLINUX)
	    find_package(ICU REQUIRED)
	    set(XML_LIBRARIES ${XML_LIBRARIES} ${ICU_LIBRARIES})
	    set(XML_INCLUDE_DIRS ${XML_INCLUDE_DIRS} ${ICU_INCLUDE_DIRS})
	elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		add_definitions(-DWINDOWS)
	endif()
else (NOT Xerces_FOUND)
	message(FATAL_ERROR "Xerces Library not found")
endif (Xerces_FOUND)

# set(CDEFS "E57_REFIMPL_REVISION_ID=${${PROJECT_NAME}_TAG}\;")
set(CDEFS "XERCES_STATIC_LIBRARY\;E57_REFIMPL_REVISION_ID=${${PROJECT_NAME}_TAG}\;")

# add_definitions(-DXERCES_STATIC_LIBRARY)

file(GLOB PDAL_E57_SOURCES
	"E57Simple.cpp"
	"E57SimpleImpl.cpp"
	"E57Foundation.cpp"
	"E57FoundationImpl.cpp"
	"time_conversion.c"
)

PDAL_ADD_LIBRARY(${PDAL_E57_LIB_NAME} STATIC "${PDAL_E57_SOURCES}")
target_link_libraries(${PDAL_E57_LIB_NAME} ${XML_LIBRARIES})

set_target_properties(${PDAL_E57_LIB_NAME} PROPERTIES
    VERSION "${PDAL_BUILD_VERSION}"
    SOVERSION "${PDAL_API_VERSION}"
    COMPILE_DEFINITIONS ${CDEFS}
    CLEAN_DIRECT_OUTPUT 1)

set_property(GLOBAL PROPERTY _PDAL_E57_INCLUDED TRUE)

