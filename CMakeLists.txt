##
# Library dependencies:
#
#   - ffmpeg
#	- cairo

# To build create a build directory and use CMake to build a project
# of your choice.

CMAKE_MINIMUM_REQUIRED( VERSION 2.6 )
PROJECT( vidthumb CXX C )
ENABLE_TESTING( )

# include cmake files from there
SET( CMAKE_MODULE_PATH  "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

INCLUDE (CompilerSettings)
INCLUDE (Libraries)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src)

ADD_EXECUTABLE( 
  vidthumb
  
  src/main.cc
  src/stream.cc
  src/frame.cc
  src/ffmpeg_stream.cc
  src/zip_stream.cc
)
TARGET_LINK_LIBRARIES(vidthumb ${LIBRARIES})

TARGET_INCLUDE_DIRECTORIES(vidthumb PRIVATE ${FFMPEG_INCLUDE_DIR})
