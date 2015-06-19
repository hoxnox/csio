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
option(WITH_SYSTEM_GLOG "Use system google-log" OFF)
option(WITH_SYSTEM_ZMQ  "Use system zeromq" OFF)
option(CSIO_FORCE_SHARED_CRT
	"Use shared run-time lib even when csio is built as static lib." OFF)

########################################################################
# general

project(csio)

include("${PROJECT_SOURCE_DIR}/cmake/ext/nx_utils.cmake")
nx_configure()


set(csio_VERSION_MAJOR 0)
set(csio_VERSION_MINOR 1)
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

	####################################################################
	# zmq
	if (WITH_SYSTEM_ZMQ)
		set(ZMQ_USE_STATIC_LIBS true)
		find_package(ZMQ REQUIRED)
	else()
		include(ExternalProject)
		set(ZMQ_ROOT "${CMAKE_CURRENT_BINARY_DIR}/zmq")
		ExternalProject_Add(zmq
			URL "http://download.zeromq.org/zeromq-4.1.2.tar.gz"
			URL_MD5 "159c0c56a895472f02668e692d122685"
			CONFIGURE_COMMAND ./configure --enable-static --disable-shared --without-libsodium --without-libgssapi_krb5 --without-documentation --prefix <INSTALL_DIR>
			INSTALL_DIR "${ZMQ_ROOT}"
			BUILD_IN_SOURCE 1
		)
		set(ZMQ_INCLUDE_DIR ${ZMQ_ROOT}/include)
		set(ZMQ_LIBRARIES ${ZMQ_ROOT}/lib/libzmq.a)
		list(APPEND EXTERNAL_DEPS zmq)
	endif()

	####################################################################
	# glog
	if (WITH_SYSTEM_GLOG)
		set(Glog_USE_STATIC_LIBS true)
		find_package(Glog REQUIRED)
	else()
		include(ExternalProject)
		set(LIBUNWIND_ROOT "${CMAKE_CURRENT_BINARY_DIR}/libunwind")
		ExternalProject_Add(libunwind
			URL "http://download.savannah.gnu.org/releases/libunwind/libunwind-1.1.tar.gz"
			URL_MD5 "fb4ea2f6fbbe45bf032cd36e586883ce"
			CONFIGURE_COMMAND ./configure --disable-minidebuginfo --enable-static --disable-shared --prefix <INSTALL_DIR>
			INSTALL_DIR "${LIBUNWIND_ROOT}"
			BUILD_IN_SOURCE 1
		)
		set(LIBUNWIND_INCLUDE_DIR ${LIBUNWIND_ROOT}/include)
		set(LIBUNWIND_LIBRARIES ${LIBUNWIND_ROOT}/lib/libunwind.a)

		set(Glog_ROOT "${CMAKE_CURRENT_BINARY_DIR}/glog")
		ExternalProject_Add(glog
			URL "https://github.com/google/glog/archive/v0.3.4.tar.gz"
			URL_MD5 "df92e05c9d02504fb96674bc776a41cb"
			CONFIGURE_COMMAND ./configure --enable-static --disable-shared --prefix <INSTALL_DIR>
			INSTALL_DIR "${Glog_ROOT}"
			BUILD_IN_SOURCE 1
		)
		set(Glog_INCLUDE_DIR ${Glog_ROOT}/include)
		set(Glog_LIBRARIES ${Glog_ROOT}/lib/libglog.a ${LIBUNWIND_LIBRARIES})
		add_dependencies(glog libunwind)
		message(STATUS "Found external dependency GLOG: " ${Glog_INCLUDE_DIR})
		list(APPEND EXTERNAL_DEPS glog)
	endif()

	include_directories(${ZMQ_INCLUDE_DIR})
	include_directories(${Glog_INCLUDE_DIR})
	list(APPEND LIBRARIES ${Glog_LIBRARIES} ${ZMQ_LIBRARIES} pthread rt)
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
	set(DZIP_SRC ./src/dzip.cpp)
	add_executable(dzip ${DZIP_SRC})
	add_dependencies(dzip_internal ${EXTERNAL_DEPS})
	target_link_libraries(dzip dzip_internal ${LIBRARIES})
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


INSTALL(TARGETS csio DESTINATION lib)
if (WITH_dzip)
	INSTALL(TARGETS dzip DESTINATION bin)
endif()
INSTALL(FILES
	./include/csio.h
	./include/csio_config.h
	DESTINATION include)
SET(CPACK_PACKAGE_NAME "csio")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libz-dev")
SET(CPACK_RPM_PACKAGE_REQUIRES "zlib-devel")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Compressed Input Output Library")
SET(CPACK_PACKAGE_VENDOR "Merder Kim <hoxnox@gmail.com>")
SET(CPACK_PACKAGE_CONTACT ${CPACK_PACKAGE_VENDOR})
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.markdown")
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
SET(CPACK_PACKAGE_VERSION_MAJOR ${CSIO_VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${CSIO_VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${CSIO_VERSION_PATCH})
SET(CPACK_DEBIAN_PACKAGE_SECTION "Utilities")
SET(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
SET(CPACK_RPM_PACKAGE_LICENSE "MIT")
SET(CPACK_STRIP_FILES TRUE)
INCLUDE(CPack)

