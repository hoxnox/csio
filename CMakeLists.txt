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
option(WITH_SYSTEM_ZMQ  "Use system zeromq" OFF)
option(WITH_CONAN "Use conan as a dependency manager" OFF)
option(CSIO_FORCE_SHARED_CRT
	"Use shared run-time lib even when csio is built as static lib." OFF)
option(WITH_GREP "Build naive grep utility" OFF)

########################################################################
# general

project(csio)

include("${PROJECT_SOURCE_DIR}/cmake/ext/nx_utils.cmake")
nx_configure()


set(csio_VERSION_MAJOR 0)
set(csio_VERSION_MINOR 1)
set(csio_VERSION_PATCH 2)
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

if (NOT WITH_CONAN)
	find_package(ZLIB REQUIRED)
	include_directories(${ZLIB_INCLUDE_DIRS})
	list(APPEND LIBRARIES ${ZLIB_LIBRARIES})
endif()

########################################################################
# conan

if (WITH_CONAN)
	include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
	conan_basic_setup()
	list(APPEND LIBRARIES ${CONAN_LIBS})
endif()

########################################################################
# dzip

if(WITH_dzip)

	if (NOT WITH_CONAN)
		####################################################################
		# zmq
		if (WITH_SYSTEM_ZMQ)
			set(ZMQ_USE_STATIC_LIBS true)
			find_package(ZMQ REQUIRED)
		else()
			include(ExternalProject)
			set(ZMQ_ROOT "${CMAKE_CURRENT_BINARY_DIR}/zmq")
			ExternalProject_Add(zmq
				URL "${PROJECT_SOURCE_DIR}/third-party/zeromq-4.1.2.tar.gz"
				URL_MD5 "159c0c56a895472f02668e692d122685"
				PATCH_COMMAND ./autogen.sh
				CONFIGURE_COMMAND ./configure --enable-static --disable-shared --without-libsodium --without-libgssapi_krb5 --without-documentation --prefix <INSTALL_DIR>
				INSTALL_DIR "${ZMQ_ROOT}"
				BUILD_IN_SOURCE 1
			)
			set(ZMQ_INCLUDE_DIR ${ZMQ_ROOT}/include)
			set(ZMQ_LIBRARIES ${ZMQ_ROOT}/lib/libzmq.a)
			list(APPEND EXTERNAL_DEPS zmq)
		endif()

		include_directories(${ZMQ_INCLUDE_DIR})
		list(APPEND LIBRARIES ${ZMQ_LIBRARIES})
	endif()

	find_package(Threads REQUIRED)
	list(APPEND LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

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
	if (EXTERNAL_DEPS AND NOT WITH_CONAN)
		add_dependencies(dzip_internal ${EXTERNAL_DEPS})
	endif()
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
# grep

if (WITH_GREP)
	add_executable(csio_grep misc/grep.c)
	target_link_libraries(csio_grep ${LIBRARIES})
endif()

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
		./src/Messages.hpp
		./src/Messages.cpp
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

