#-------------------------------------------------------------------------------------------------------------
# Find the required packages and create variables:
# TARGET_INCLUDE_DIRS - include directories for MY_TARGET
# TARGET_LINK_DIRS - link directoies for MY_TARGET
# TARGET_LIBRARIES - libraries MY_TARGET should link to

# BWAPI
FIND_PACKAGE(BWAPI REQUIRED)
SET(TARGET_INCLUDE_DIRS ${TARGET_INCLUDE_DIRS} ${BWAPI_INCLUDE_DIRS}) 
SET(TARGET_LINK_DIRS ${TARGET_LINK_DIRS} ${BWAPI_LIBRARY_DIRS})
SET(TARGET_LIBRARIES ${TARGET_LIBRARIES} ${BWAPI_LIBRARIES})

# Boost
SET(Boost_USE_STATIC_LIBS ON)
FIND_PACKAGE(Boost REQUIRED thread)
SET(TARGET_INCLUDE_DIRS ${TARGET_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
SET(TARGET_LINK_DIRS ${TARGET_LINK_DIRS} ${Boost_LIBRARY_DIRS})
SET(TARGET_LIBRARIES ${TARGET_LIBRARIES} ${Boost_LIBRARIES})

# CGAL
IF ($ENV{CGAL_DIR})
  SET(CGAL_DIR $ENV{CGAL_DIR})
ELSE ()
  FIND_PATH(CGAL_DIR CGALConfig.cmake
    PATHS $ENV{ProgramFiles}/CGAL*/
    DOC "Location of CGAL installation directory")
ENDIF ()
 
FIND_PACKAGE(CGAL REQUIRED)
INCLUDE(${CGAL_USE_FILE})

#-------------------------------------------------------------------------------------------------------------
# Setup dirs
INCLUDE_DIRECTORIES(${TARGET_INCLUDE_DIRS})
LINK_DIRECTORIES(${TARGET_LINK_DIRS})

#-------------------------------------------------------------------------------------------------------------
# Create target

OPTION(BWTA_DEBUG_DRAW "Use Qt GUI for debug draw stuff" OFF)
IF (BWTA_DEBUG_DRAW)
  ADD_DEFINITIONS(-DBWTA_DEBUG_DRAW=1)
ELSE ()
  ADD_DEFINITIONS(-DBWTA_DEBUG_DRAW=0)
ENDIF ()

SET(SOURCES 
  BaseLocationImpl.cpp
  BWTA.cpp
  BWTA_Result.cpp
  calculate_connectivity.cpp
  ChokepointImpl.cpp
  ConnectedComponent.cpp
  extract_polygons.cpp
  find_base_locations.cpp
  get_voronoi_edges.cpp
  Graph.cpp
  load_data.cpp
  Node.cpp
  pathfinding.cpp
  Polygon.cpp
  RegionImpl.cpp
  terrain_analysis.cpp
  util.cpp
  VertexData.cpp
)

INCLUDE_DIRECTORIES( 
  ../include
)

ADD_DEFINITIONS(
  -D_CRT_SECURE_NO_WARNINGS
)

IF (BWTA_FAST_BUILD)
  ADD_LIBRARY(BWTA_cmake STATIC fastbuild.cpp)
ELSE ()
  ADD_LIBRARY(BWTA_cmake STATIC ${SOURCES})
ENDIF ()

TARGET_LINK_LIBRARIES(BWTA_cmake ${TARGET_LIBRARIES})

SET_TARGET_PROPERTIES(BWTA_cmake PROPERTIES PROJECT_LABEL BWTA)
SET_TARGET_PROPERTIES(BWTA_cmake PROPERTIES OUTPUT_NAME BWTA)
