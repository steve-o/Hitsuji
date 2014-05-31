# CMake build script for Velocity Analytics platform plugin
# x64 Windows Server-only
# 2014/05/22 -- Steven.McCoy@thomsonreuters.com

cmake_minimum_required (VERSION 2.8.8)

project (Hitsuji)

set(HITSUJI_VERSION_MAJOR 1)
set(HITSUJI_VERSION_MINOR 0)
set(HITSUJI_VERSION_BUILD 0)

# Net-SNMP SDK installed, tested 5.7.2.
if (MSVC11)
	set(NETSNMP_BUILD_COMPILER "vc110")
elseif (MSVC10)
	set(NETSNMP_BUILD_COMPILER "vc100")
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(NETSNMP_BUILD_TYPE "debug")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(NETSNMP_BUILD_TYPE "release")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(NETSNMP_ROOT D:/net-snmp-5.7.2/stage/${NETSNMP_BUILD_COMPILER}/x64/${NETSNMP_BUILD_TYPE})
set(NETSNMP_INCLUDE_DIRS ${NETSNMP_ROOT}/include)
set(NETSNMP_LIBRARY_DIRS ${NETSNMP_ROOT}/lib)
set(NETSNMP_LIBRARIES
	netsnmp.lib
	netsnmpagent.lib
	netsnmpmibs.lib
)

# TREP-VA 7 SDK
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(VHAYU_BUILD_TYPE "debug")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(VHAYU_BUILD_TYPE "release")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(VHAYU_ROOT D:/Vhayu-7.0.5)
set(VHAYU_INCLUDE_DIRS
	${VHAYU_ROOT}/Engine/TBSDK/Include
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/include
)
set(VHAYU_LIBRARY_DIRS
	${VHAYU_ROOT}/Engine/TBSDK/Lib64/${VHAYU_BUILD_TYPE}
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/lib
)
set(VHAYU_XERCES_LIBRARY
	optimized xerces-c_3.lib
	debug xerces-c_3D.lib
)
set(VHAYU_LIBRARIES
	FlexRecord.lib
	PluginFramework.lib
	Utility.lib
	SEDll.lib
	${VHAYU_XERCES_LIBRARY}
)

# Thomson Reuters Ultra Performance API
if (MSVC11)
	set(UPA_BUILD_COMPILER "VS110")
elseif (MSVC10)
	set(UPA_BUILD_COMPILER "VS100")
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(UPA_BUILD_TYPE "Debug_MDd")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(UPA_BUILD_TYPE "Release_MD")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(UPA_ROOT D:/upa7.6.0.L1.win.rrg)
set(UPA_INCLUDE_DIRS
	${UPA_ROOT}/Include
	${UPA_ROOT}/Include/rwf
)
set(UPA_LIBRARY_DIRS ${UPA_ROOT}/Libs/WIN_64_${UPA_BUILD_COMPILER}/${UPA_BUILD_TYPE})
set(UPA_LIBRARY_DIR ${UPA_LIBRARY_DIRS})
set(UPA_LIBRARIES
	librsslData
	librsslMessages
	librsslTransport
)

# ZeroMQ SDK 2.2.0
set(ZEROMQ_ROOT "C:/Program Files/ZeroMQ 2.2.0")
set(ZEROMQ_INCLUDE_DIRS "${ZEROMQ_ROOT}/include")
set(ZEROMQ_LIBRARY_DIRS "${ZEROMQ_ROOT}/lib")
set(ZEROMQ_LIBRARIES
	optimized libzmq-v100-mt.lib
	debug libzmq-v100-mt-gd.lib
)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(ZEROMQ_RUNTIME_LIBRARIES "${ZEROMQ_ROOT}/bin/libzmq-v100-mt-gd.dll")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(ZEROMQ_RUNTIME_LIBRARIES "${ZEROMQ_ROOT}/bin/libzmq-v100-mt.dll")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

# Simple Binary Encoding
set(SBE_ROOT "${CMAKE_SOURCE_DIR}/third_party/simple-binary-encoding")
set(SBE_INCLUDE_DIRS
	${SBE_ROOT}/include
	${CMAKE_SOURCE_DIR}/include/sbe
)

# Boost headers plus built libraries
set(BOOST_ROOT D:/boost_1_54_0)
set(BOOST_LIBRARYDIR ${BOOST_ROOT}/stage/lib)
set(Boost_USE_STATIC_LIBS ON)
find_package (Boost 1.50 COMPONENTS atomic chrono thread REQUIRED)

# Python for source generators
find_package(PythonInterp REQUIRED)

option(CONFIG_AS_APPLICATION
	"Build an application instead of a library." ON)

#-----------------------------------------------------------------------------
# force off-tree build

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
message(FATAL_ERROR "CMake generation is not allowed within the source directory!
Remove the CMakeCache.txt file and try again from another folder, e.g.:

   del CMakeCache.txt
   mkdir build
   cd build
   cmake ..
")
endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

#-----------------------------------------------------------------------------
# default to Release build, frequently broken: manually run cmake-gui to fix

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

#-----------------------------------------------------------------------------
# platform specifics

add_definitions(
	-DWIN32
	-DWIN32_LEAN_AND_MEAN
# Windows Server 2008 R2 and Windows 7
	-D_WIN32_WINNT=0x0601
# Net-SNMP Autoconf overrides
	-DHAVE_STDINT_H
	-DHAVE_SOCKLEN_T
# UPA version
        -DUPA_LIBRARY_VERSION="7.6.0."
# production release
	-DOFFICIAL_BUILD
##	-DENABLE_LEAK_TRACKER
)

if(CONFIG_AS_APPLICATION)
	add_definitions(
		-DCONFIG_AS_APPLICATION
	)
endif(CONFIG_AS_APPLICATION)

# SEH Exceptions.
string(REGEX REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Parallel make.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Optimization flags.
# http://msdn.microsoft.com/en-us/magazine/cc301698.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")

# Eliminate duplicate strings.
# http://msdn.microsoft.com/en-us/library/s0s0asdt.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GF")

# Enable function-level linking.
# http://msdn.microsoft.com/en-us/library/xsa71f43.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Gy")

if (MSVC12)
# REF and ICF linker optimisations to also work on identical data COMDATs.
# http://blogs.msdn.com/b/vcblog/archive/2013/09/11/introducing-gw-compiler-switch.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Gw")
endif ()

# Disable buffer security check.
# http://msdn.microsoft.com/en-us/library/8dbf701c.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GS-")

# Debug optimized builds.
# http://randomascii.wordpress.com/2013/09/11/debugging-optimized-codenew-in-visual-studio-2012/
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /d2Zi+")

#-----------------------------------------------------------------------------
# Generated source files

configure_file(
	${CMAKE_SOURCE_DIR}/version_generator.py.in
	${CMAKE_BINARY_DIR}/version_generator.py
	@ONLY
)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/version.cc
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_BINARY_DIR}/version_generator.py > ${CMAKE_BINARY_DIR}/version.cc
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	DEPENDS ${CMAKE_BINARY_DIR}/version_generator.py
)
configure_file(
	${CMAKE_SOURCE_DIR}/src/version.hh
	${CMAKE_BINARY_DIR}/version.hh
	COPYONLY
)
configure_file(
	${CMAKE_SOURCE_DIR}/version.rc.in
	${CMAKE_BINARY_DIR}/version.rc
	@ONLY
)

#-----------------------------------------------------------------------------
# source files

set(cxx-sources
	src/client.cc
	src/config.cc
	src/hitsuji.cc
	src/main.cc
	src/plugin.cc
	src/provider.cc
	src/upa.cc
	src/upaostream.cc
	src/vta_bar.cc
	src/vta_test.cc
	src/chrome/common/json_schema_constants.cc
	src/chrome/common/json_schema_validator.cc
	src/chromium/chromium_switches.cc
	src/chromium/command_line.cc
	src/chromium/debug/stack_trace.cc
	src/chromium/debug/stack_trace_win.cc
	src/chromium/file_util.cc
	src/chromium/file_util_win.cc
	src/chromium/json/json_reader.cc
	src/chromium/json/json_writer.cc
	src/chromium/json/string_escape.cc
	src/chromium/memory/singleton.cc
	src/chromium/metrics/histogram.cc
	src/chromium/metrics/stats_counters.cc
	src/chromium/metrics/stats_table.cc
	src/chromium/logging.cc
	src/chromium/logging_win.cc
	src/chromium/shared_memory_win.cc
	src/chromium/string_number_conversions.cc
	src/chromium/string_piece.cc
	src/chromium/string_split.cc
	src/chromium/string_util.cc
	src/chromium/stringprintf.cc
	src/chromium/synchronization/lock.cc
	src/chromium/synchronization/lock_impl_win.cc
	src/chromium/values.cc
	src/chromium/vlog.cc
	src/chromium/win/event_trace_provider.cc
	src/googleurl/url_parse.cc
	${CMAKE_BINARY_DIR}/version.cc
)

set(rc-sources
	${CMAKE_BINARY_DIR}/version.rc
)

include_directories(
	include
	${CMAKE_CURRENT_BINARY_DIR}
	${NETSNMP_INCLUDE_DIRS}
	${VHAYU_INCLUDE_DIRS}
	${UPA_INCLUDE_DIRS}
	${ZEROMQ_INCLUDE_DIRS}
	${SBE_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS}
)

link_directories(
	${NETSNMP_LIBRARY_DIRS}
	${VHAYU_LIBRARY_DIRS}
	${UPA_LIBRARY_DIRS}
	${ZEROMQ_LIBRARY_DIRS}
	${Boost_LIBRARY_DIRS}
)

#-----------------------------------------------------------------------------
# output

if(CONFIG_AS_APPLICATION)
	add_executable(Hitsuji ${cxx-sources} ${rc-sources})
	target_link_libraries(Hitsuji
		${NETSNMP_LIBRARIES}
		${UPA_LIBRARIES}
		${Boost_LIBRARIES}
		${ZEROMQ_LIBRARIES}
		ws2_32.lib
		wininet.lib
		dbghelp.lib
	)
	set(config
		${CMAKE_CURRENT_SOURCE_DIR}/config/Hitsuji.json
		${CMAKE_CURRENT_SOURCE_DIR}/config/example.reg
		"${CMAKE_CURRENT_SOURCE_DIR}/config/Sawbuck ETW provider.reg"
	)
else(CONFIG_AS_APPLICATION)
	add_library(Hitsuji SHARED ${cxx-sources} ${rc-sources})
	target_link_libraries(Hitsuji
		${NETSNMP_LIBRARIES}
		${VHAYU_LIBRARIES}
		${UPA_LIBRARIES}
		${Boost_LIBRARIES}
		${ZEROMQ_LIBRARIES}
		ws2_32.lib
		wininet.lib
		dbghelp.lib
	)
	set(config
		${CMAKE_CURRENT_SOURCE_DIR}/config/Hitsuji.xml
		${CMAKE_CURRENT_SOURCE_DIR}/config/example.reg
		"${CMAKE_CURRENT_SOURCE_DIR}/config/Sawbuck ETW provider.reg"
	)
endif(CONFIG_AS_APPLICATION)

file(GLOB mibs "${CMAKE_CURRENT_SOURCE_DIR}/mibs/*.txt")

install (TARGETS Hitsuji DESTINATION bin)
install (FILES ${ZEROMQ_RUNTIME_LIBRARIES} DESTINATION bin)
install (FILES ${config} DESTINATION config)
install (FILES ${mibs} DESTINATION mibs)

set (CPACK_SOURCE_IGNORE_FILES "*.lib")

set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
if(CONFIG_AS_APPLICATION)
	set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Velocity Analytics hosted analytics application")
else(CONFIG_AS_APPLICATION)
	set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Velocity Analytics hosted analytics plugin")
endif(CONFIG_AS_APPLICATION)
set (CPACK_PACKAGE_VENDOR "Thomson Reuters")
set (CPACK_NSIS_CONTACT "Steven McCoy <Steven.McCoy@thomsonreuters.com>")
# There is a bug in NSI that does not handle full unix paths properly. Make
# sure there is at least one set of four (4) backlasshes.
set (CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\branding.bmp")
set (CPACK_NSIS_COMPRESSOR "/SOLID lzma")
set (CPACK_PACKAGE_VERSION_MAJOR "${HITSUJI_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${HITSUJI_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${HITSUJI_VERSION_BUILD}")
include (CPack)

# end of file
