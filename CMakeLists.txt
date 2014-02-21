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
configure_file(
	"${PROJECT_SOURCE_DIR}/src/csio_config.cfg"
	"${PROJECT_SOURCE_DIR}/src/csio_config.h"
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
	find_package(Threads REQUIRED)
	if (NOT CMAKE_USE_PTHREADS_INIT)
		message(FATAL_ERROR "dzip utility needs pthreads")
	endif()
	set(DZIP_SRC ./src/dzip.c)
	add_executable(dzip ${DZIP_SRC})
	target_link_libraries(dzip pthread)
	set_target_properties(dzip PROPERTIES COMPILE_FLAGS "-std=c99")
endif()

########################################################################
# csio

set(CSIO_SRC include/csio.h
             src/csio.c)
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
		./test/tcsio_dictzip.hpp)
	enable_testing()
	find_package(GTest REQUIRED)
	include_directories(${GTEST_INCLUDE_DIRS})
	set(TEST test_${PROJECT_NAME})
	add_executable(${TEST} ${SOURCES_TEST})
	if(NOT WIN32)
		list(APPEND LIBRARIES pthread)
	endif(NOT WIN32)
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

