# @author Merder Kim <hoxnox@gmail.com>
# @date 20140221 15:40:20
# csio cmake build script

cmake_minimum_required(VERSION 2.6.4)

########################################################################
# options

option(WITH_TESTS "Build tests."  OFF)
option(WITH_DOCS  "Generate docs." OFF)
option(WITH_SHARED_LIBS "Build shared libraries." OFF)
option(WITH_STATIC_LIBS "Build static libraries." ON)
option(WITH_dzip        "Build dzip compress utility" ON)
option(CSIO_FORCE_SHARED_CRT
	"Use shared run-time lib even when csio is built as static lib." OFF)

########################################################################
# general

project(csio)

include("${PROJECT_SOURCE_DIR}/cmake/ext/nx_utils.cmake")
nx_configure()


set(csio_VERSION_MAJOR 0)
set(csio_VERSION_MINOR 0)
set(csio_VERSION_PATCH 1)
# configuration header
set(TEST_SAMPLES_DIR "${PROJECT_SOURCE_DIR}/test/samples/")
set(TEST_TMP_DIR "/tmp")
configure_file(
	"${PROJECT_SOURCE_DIR}/src/csio_config.cfg"
	"${PROJECT_SOURCE_DIR}/include/csio_config.h"
	ESCAPE_QUOTES
)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}"
                      "${PROJECT_SOURCE_DIR}/cmake/Modules")

include_directories("${PROJECT_SOURCE_DIR}/src"
                    "${PROJECT_SOURCE_DIR}/include")

########################################################################
# zlib

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
list(APPEND LIBRARIES ${ZLIB_LIBRARIES})

########################################################################
# dzip

if(WITH_dzip)
	set(ZMQ_USE_STATIC_LIBS true)
	find_package(ZMQ REQUIRED)
	set(Glog_USE_STATIC_LIBS true)
	find_package(Glog REQUIRED)
	include_directories(${ZMQ_INCLUDE_DIR})
	include_directories(${Glog_INCLUDE_DIR})
	list(APPEND LIBRARIES ${ZMQ_LIBRARIES} ${Glog_LIBRARIES} pthread rt)
	find_package(Threads REQUIRED)
	if (NOT CMAKE_USE_PTHREADS_INIT)
		message(FATAL_ERROR "dzip utility needs pthreads")
	endif()
	add_library(dzip_internal STATIC 
		./src/ProcessManagerBase.hpp
		./src/ProcessManagerBase.cpp
		./src/Messages.hpp
		./src/Messages.cpp
		./src/CompressManager.hpp
		./src/CompressManager.cpp
		./src/Writer.hpp
		./src/Writer.cpp
		./src/Config.hpp
		./src/Config.cpp
		./src/Compressor.hpp
		./src/Compressor.cpp
	)
	list(APPEND LIBRARIES dzip_internal)
	set(DZIP_SRC ./src/dzip.cpp)
	add_executable(dzip ${DZIP_SRC})
	target_link_libraries(dzip_internal ${LIBRARIES})
	target_link_libraries(dzip ${LIBRARIES})
	set_target_properties(dzip PROPERTIES COMPILE_FLAGS "-std=c++0x")
	set_target_properties(dzip_internal PROPERTIES COMPILE_FLAGS "-std=c++0x")
endif()

########################################################################
# csio

set(CSIO_SRC include/csio.h src/csio.c)
nx_add_library(csio ${CSIO_FORCE_SHARED_CRT} ${WITH_STATIC_LIBS}
	${WITH_SHARED_LIBS} "CSIO_SHARED"
	"${csio_VERSION_MAJOR}-${csio_VERSION_MINOR}-${csio_VERSION_PATCH}"
	"${CSIO_SRC}")
list(APPEND LIBRARIES csio)

########################################################################
# docs

if(WITH_DOCS)
	FIND_PACKAGE(Doxygen)
	if(DOXYGEN_FOUND)
		set(ENV{csio_ROOT} "${PROJECT_SOURCE_DIR}")
		ADD_CUSTOM_TARGET(doc COMMAND ${CMAKE_COMMAND}
			-D PDIR="${PROJECT_SOURCE_DIR}"
			-P "${PROJECT_SOURCE_DIR}/doc/doxygen.cmake")
	else()
		message(WARNING
			"Doxygen not found."
			" Documentation wouldn't be ganarated")
	endif()
endif()

########################################################################
# tests

if(WITH_TESTS)
	set(SOURCES_TEST
		./test/test.cpp
		./test/tcsio_none.hpp
		./test/tcsio_dictzip.hpp
	)
	enable_testing()
	find_package(GTest REQUIRED)
	include_directories(${GTEST_INCLUDE_DIRS})
	set(TEST test_${PROJECT_NAME})
	add_executable(${TEST} ${SOURCES_TEST})
	add_executable(access_speed_test ./test/access_speed_test.c)
	set_target_properties(access_speed_test PROPERTIES COMPILE_FLAGS "-std=c1x")
	target_link_libraries(access_speed_test ${LIBRARIES})
	set_target_properties(${TEST} PROPERTIES COMPILE_FLAGS "-std=c++0x")
	target_link_libraries("${TEST}" ${GTEST_LIBRARIES} ${LIBRARIES})
	nx_GTEST_ADD_TESTS("${TEST}" ${SOURCES_TEST})

endif()

########################################################################
# installation

set(CPACK_SET_DESTDIR ON)

install(TARGETS ${TARGETS} DESTINATION bin)
#install(FILES <files> DESTINATION <dest>)

INCLUDE(InstallRequiredSystemLibraries)

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "csio DESC")
SET(CPACK_PACKAGE_VENDOR "ORG")
SET(CPACK_PACKAGE_DESCRIPTION_FILE
	"${CMAKE_CURRENT_SOURCE_DIR}/README.markdown")
SET(CPACK_RESOURCE_FILE_LICENSE
	"${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
SET(CPACK_PACKAGE_VERSION_MAJOR ${csio_VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${csio_VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${csio_VERSION_PATCH})
SET(CPACK_PACKAGE_INSTALL_DIRECTORY
	"CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")
IF(WIN32 AND NOT UNIX)
	# NOT IMPLEMENTED
ELSE(WIN32 AND NOT UNIX)
	SET(CPACK_STRIP_FILES ${TARGETS})
	SET(CPACK_SOURCE_STRIP_FILES "")
ENDIF(WIN32 AND NOT UNIX)
#SET(CPACK_PACKAGE_EXECUTABLES "null" "null desc")

INCLUDE(CPack)

