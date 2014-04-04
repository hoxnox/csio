# Find Snappy
# Merder Kim <hoxnox@gmail.com>
# 
# input:
#  Glog_ROOT
#  Glog_USE_STATIC_LIBS
#
# output:
#  Glog_FOUND
#  Glog_INCLUDE_DIR
#  Glog_LIBRARIES
#

if(Glog_INCLUDE_DIR AND Glog_LIBRARIES)
	set(Glog_FIND_QUITELY TRUE) # cached
endif(Glog_INCLUDE_DIR AND Glog_LIBRARIES)

if(NOT DEFINED Glog_ROOT)
	set(Glog_ROOT /usr /usr/local $ENV{Glog_ROOT})
endif(NOT DEFINED Glog_ROOT)

find_path(Glog_INCLUDE_DIR logging.h
	PATHS ${Glog_ROOT}
	PATH_SUFFIXES glog glog/include include
)

if( Glog_USE_STATIC_LIBS )
	set( _glog_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
	if(WIN32)
		set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
	else()
		set(CMAKE_FIND_LIBRARY_SUFFIXES .a )
	endif()
endif()

find_library(Glog_LIBRARIES
	NAMES glog
	PATHS ${Glog_ROOT}
	PATH_SUFFIXES lib
)
mark_as_advanced(Glog_INCLUDE_DIR Glog_LIBRARIES)

# Restore the original find library ordering
if( Glog_USE_STATIC_LIBS )
	set(CMAKE_FIND_LIBRARY_SUFFIXES ${_glog_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

include("${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake")
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Glog DEFAULT_MSG Glog_INCLUDE_DIR Glog_LIBRARIES)


