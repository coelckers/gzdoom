cmake_minimum_required( VERSION 3.1.0 )
project(GZDoom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 


if( COMMAND cmake_policy )
	if( POLICY CMP0011 )
		cmake_policy( SET CMP0011 NEW )
	endif()
	if( POLICY CMP0054 )
		cmake_policy( SET CMP0054 NEW )
	endif()
endif()

list( APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )
include( FindPackageHandleStandardArgs )

# Support cross compiling
option( FORCE_CROSSCOMPILE "Turn on cross compiling." NO )
if( FORCE_CROSSCOMPILE )
	set( CMAKE_CROSSCOMPILING TRUE )
endif()

if(CMAKE_CROSSCOMPILING)
	set(IMPORT_EXECUTABLES "IMPORTFILE-NOTFOUND" CACHE FILEPATH "Export file from native build.")
	include(${IMPORT_EXECUTABLES})
endif()

# Recursive function to place PK3 archive source files into a hierarchy of source file in the IDE
function( assort_pk3_source_folder FOLDER_NAME PK3_DIR )
	# Assort source files into folders in the IDE
	file(GLOB PK3_SRCS ${PK3_DIR}/*) # Create list of all files in this folder
	foreach(PK3_SRC ${PK3_SRCS})
		# If there are subfolders, recurse into them
		if(IS_DIRECTORY ${PK3_SRC})
			get_filename_component(DIRNAME ${PK3_SRC} NAME)
			# Exclude folder from list of source files
			list(REMOVE_ITEM PK3_SRCS ${PK3_SRC})
			# Recurse deeper into the filesystem folder tree
			assort_pk3_source_folder( ${FOLDER_NAME}\\${DIRNAME} ${PK3_SRC} )
		endif()
		# Assign IDE group for current top-level source files
		source_group(${FOLDER_NAME} FILES ${PK3_SRCS})
	endforeach()
endfunction()

option( PK3_QUIET_ZIPDIR "Do not list files processed by zipdir" NO )
if( PK3_QUIET_ZIPDIR )
	set( PK3_ZIPDIR_OPTIONS "-q" )
endif()

# Simplify pk3 building, add_pk3(filename srcdirectory)
function( add_pk3 PK3_NAME PK3_DIR )
	# Generate target name. Just use "pk3" for main pk3 target.
	string( REPLACE "." "_" PK3_TARGET ${PK3_NAME} )

	if( NOT ZDOOM_OUTPUT_OLDSTYLE )
		add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
			COMMAND zipdir -udf ${PK3_ZIPDIR_OPTIONS} ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} ${PK3_DIR}
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} $<TARGET_FILE_DIR:zdoom>/${PK3_NAME}
			DEPENDS zipdir )
	else()
		add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
			COMMAND zipdir -udf ${PK3_ZIPDIR_OPTIONS} ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} ${PK3_DIR}
			DEPENDS zipdir )
	endif()
	# Create a list of source files for this PK3, for use in the IDE
	# Phase 1: Create a list of all source files for this PK3 archive, except
	#  for a couple of strife image file names that confuse CMake.
	file(GLOB_RECURSE PK3_SRCS ${PK3_DIR}/*)
	# Exclude from the source list some files with brackets in the
	# file names here, because they confuse CMake.
	# This only affects the list of source files shown in the IDE.
	# It does not actually remove the files from the PK3 archive.
	# First replace that toxic bracket character with something we can handle
	string(REPLACE "[" confusing_bracket PK3_SRCS "${PK3_SRCS}")
	string(REPLACE "]" confusing_bracket PK3_SRCS "${PK3_SRCS}")
	foreach(PK3_SRC ${PK3_SRCS}) # All source files at all levels
		# Exclude those quarantined source file source file names that once had a bracket
		if(${PK3_SRC} MATCHES confusing_bracket)
			# message(STATUS "Ignoring PK3 file name containing brackets "${PK3_SRC})
			list(REMOVE_ITEM PK3_SRCS ${PK3_SRC})
		endif()
	endforeach()
	# Phase 2: Create the PK3 build rule, including the source file list for the IDE
	# Touch the zipdir executable here so that the pk3s are forced to
	# rebuild each time since their dependency has "changed."
	add_custom_target( ${PK3_TARGET} ALL
		COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE:zipdir>
		DEPENDS ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
		SOURCES ${PK3_SRCS})
	# Phase 3: Assign source files to a nice folder structure in the IDE
	assort_pk3_source_folder("Source Files" ${PK3_DIR})
	# Phase 4: Add the resulting PK3 to the install target.
	if( WIN32 )
		set( INSTALL_PK3_PATH . CACHE STRING "Directory where zdoom.pk3 will be placed during install." )
	else()
		set( INSTALL_PK3_PATH share/games/doom CACHE STRING "Directory where zdoom.pk3 will be placed during install." )
	endif()
	install(FILES "${PROJECT_BINARY_DIR}/${PK3_NAME}"
			DESTINATION ${INSTALL_PK3_PATH}
			COMPONENT "Game resources")
endfunction()

# Macro for building libraries without debugging information
macro( make_release_only )
	set( CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_RELEASE} )
	set( CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELEASE} )
	string( REPLACE "/MT " "/MTd " CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_RELEASE} )
	set( CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_RELEASE} )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELEASE} )
	string( REPLACE "/MT " "/MTd " CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_RELEASE} )
endmacro()

IF( NOT CMAKE_BUILD_TYPE )
	SET( CMAKE_BUILD_TYPE Debug CACHE STRING
		"Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
		FORCE )
ENDIF()

set( ZDOOM_OUTPUT_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Directory where zdoom.pk3 and the executable will be created." )
set( ZDOOM_EXE_NAME "gzdoom" CACHE FILEPATH "Name of the executable to create" )
if( MSVC )
	# Allow the user to use ZDOOM_OUTPUT_DIR as a single release point.
	# Use zdoom, zdoomd, zdoom64, and zdoomd64 for the binary names
	option( ZDOOM_OUTPUT_OLDSTYLE "Don't use Release/Debug directories." OFF )
else()
	set( ZDOOM_OUTPUT_OLDSTYLE OFF )
endif()

# Replacement variables for a possible long list of C/C++ compilers compatible with GCC
if( "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" MATCHES "Clang" )
	set( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE TRUE )
else()
	set( ZD_CMAKE_COMPILER_IS_GNUC_COMPATIBLE FALSE )
endif()

if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" )
	set( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE TRUE )
else()
	set( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE FALSE )
endif()

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	set( PROFILE 0 CACHE BOOL "Enable profiling with gprof for Debug and RelWithDebInfo build types." )

	if( CMAKE_CXX_STANDARD )
		# By default, try_compile() invocations ignore CXX_STANDARD value
		# CMake 3.8 and newer have CMP0067 policy that fixes the issue, but it should be enabled explicitly
		# Older versions require manual specification of the corresponding command line option
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${CMAKE_CXX_STANDARD}" )
	endif()
endif()

# Fast math flags, required by some subprojects
set( ZD_FASTMATH_FLAG "" )
if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	set( ZD_FASTMATH_FLAG "-ffast-math -ffp-contract=fast" )
elseif( MSVC )
	set( ZD_FASTMATH_FLAG "/fp:fast" )
endif()

macro( use_fast_math )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ZD_FASTMATH_FLAG}" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ZD_FASTMATH_FLAG}" )
endmacro()


include( CheckFunctionExists )

macro( require_stricmp )
	CHECK_FUNCTION_EXISTS( stricmp STRICMP_EXISTS )
	if( NOT STRICMP_EXISTS )
		add_definitions( -Dstricmp=strcasecmp )
	endif()
endmacro()

macro( require_strnicmp )
	CHECK_FUNCTION_EXISTS( strnicmp STRNICMP_EXISTS )
	if( NOT STRNICMP_EXISTS )
		add_definitions( -Dstrnicmp=strncasecmp )
	endif()
endmacro()

option( NO_OPENAL "Disable OpenAL sound support" OFF )

find_package( BZip2 )
find_package( JPEG )
find_package( ZLIB )

include( TargetArch )

target_architecture(ZDOOM_TARGET_ARCH)

if( ${ZDOOM_TARGET_ARCH} MATCHES "x86_64" )
	set( HAVE_VM_JIT ON )
endif()

option (HAVE_VULKAN "Enable Vulkan support" ON)

# no, we're not using external asmjit for now, we made too many modifications to our's.
# if the asmjit author uses our changes then we'll update this.

#if( ${HAVE_VM_JIT} )
#	find_package( asmjit )
#endif()

if( MSVC )
	# Eliminate unreferenced functions and data
	# Perform identical COMDAT folding
	set( REL_LINKER_FLAGS "/opt:ref /opt:icf /nodefaultlib:msvcrt /TSAWARE /LARGEADDRESSAWARE" )

	# String pooling
	# Function-level linking
	# Disable run-time type information
	if ( HAVE_VULKAN )
		set( ALL_C_FLAGS "/GF /Gy /permissive- /DHAVE_VULKAN /DHAVE_SOFTPOLY" )
	else()
		set( ALL_C_FLAGS "/GF /Gy /permissive- /DHAVE_SOFTPOLY" )
	endif()

	# Use SSE 2 as minimum always as the true color drawers needs it for __vectorcall
	#set( ALL_C_FLAGS "${ALL_C_FLAGS} /arch:SSE2") # This is already the default



	# Avoid CRT DLL dependancies in release builds, optionally generate assembly output for checking crash locations.
	option( ZDOOM_GENERATE_ASM "Generate assembly output." OFF )
	if( ZDOOM_GENERATE_ASM )
		set( REL_C_FLAGS "/MT /Oy /Oi /FAcs /GS-" )
	else()
		set( REL_C_FLAGS "/MT /Oy /Oi /GS-" )
	endif()


	# Debug allocations in debug builds
	set( DEB_C_FLAGS "/D _CRTDBG_MAP_ALLOC /MTd" )

	# Disable warnings for unsecure CRT functions from VC8+
	set( ALL_C_FLAGS "${ALL_C_FLAGS} /wd4996 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0600" )

	# The CMake configurations set /GR and /MD by default, which conflict with our settings.
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} )
	string(REPLACE "/Ob1 " "/Ob2 " CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} )
	string(REPLACE "/MDd " " " CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL} )
	string(REPLACE "/MD " " " CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO} )
	string(REPLACE "/MDd " " " CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG} )
else()
	set( REL_LINKER_FLAGS "" )
	if ( HAVE_VULKAN )
		set( ALL_C_FLAGS "-ffp-contract=off -DHAVE_VULKAN -DHAVE_SOFTPOLY" )
	else()
		set( ALL_C_FLAGS "-ffp-contract=off -DHAVE_SOFTPOLY" )
	endif()

	if ( UNIX )
		include(CheckSymbolExists)
		check_symbol_exists( "fts_set" "fts.h" HAVE_FTS )
		if ( NOT HAVE_FTS )
			include ( FindPkgConfig )
			pkg_check_modules( MUSL_FTS musl-fts )
			if ( MUSL_FTS_FOUND )
				set ( ALL_C_FLAGS "${ALL_C_FLAGS} ${MUSL_FTS_LDFLAGS}" )
			else ( MUSL_FTS_FOUND )
				message (ERROR "fts_* functions not found in the system" )
			endif ( MUSL_FTS_FOUND )
		endif ( NOT HAVE_FTS )
	endif ( UNIX )

	set( REL_C_FLAGS "" )
	set( DEB_C_FLAGS "" )


	if( APPLE )
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")
		if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
			# If we're compiling with a custom GCC on the Mac (which we know since g++-4.2 doesn't support C++11) statically link libgcc.
			set( ALL_C_FLAGS "-static-libgcc" )
		endif()
	elseif( NOT MINGW )
		# Generic GCC/Clang requires position independent executable to be enabled explicitly
		if (NOT (CMAKE_SYSTEM_NAME MATCHES "SunOS"))
			set( ALL_C_FLAGS "${ALL_C_FLAGS} -fPIE" )
			set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie" )
		endif()
	endif( APPLE )
endif()

set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${REL_LINKER_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} ${REL_LINKER_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} ${REL_LINKER_FLAGS}" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_C_FLAGS}" )
set( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${REL_C_FLAGS}" )
set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEB_C_FLAGS} -D_DEBUG" )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ALL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${REL_C_FLAGS}" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEB_C_FLAGS} -D_DEBUG" )

option(FORCE_INTERNAL_ZLIB "Use internal zlib")
option(FORCE_INTERNAL_JPEG "Use internal jpeg")
option(FORCE_INTERNAL_BZIP2 "Use internal bzip2")
option(FORCE_INTERNAL_ASMJIT "Use internal asmjit" ON)
mark_as_advanced( FORCE_INTERNAL_ASMJIT )

if (HAVE_VULKAN)
	add_subdirectory( libraries/glslang/glslang)
	add_subdirectory( libraries/glslang/spirv )
	add_subdirectory( libraries/glslang/OGLCompilersDLL )
endif()

if( ZLIB_FOUND AND NOT FORCE_INTERNAL_ZLIB )
	message( STATUS "Using system zlib, includes found at ${ZLIB_INCLUDE_DIR}" )
else()
	message( STATUS "Using internal zlib" )
	set( SKIP_INSTALL_ALL TRUE ) # Avoid installing zlib
	add_subdirectory( libraries/zlib )
	set( ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/zlib )
	set( ZLIB_LIBRARIES z )
	set( ZLIB_LIBRARY z )
endif()

if( HAVE_VM_JIT AND UNIX )
	check_symbol_exists( "backtrace" "execinfo.h" HAVE_BACKTRACE )
	if( NOT HAVE_BACKTRACE )
		set( CMAKE_REQUIRED_FLAGS "-lexecinfo" )
		check_symbol_exists( "backtrace" "execinfo.h" HAVE_LIBEXECINFO )
		if( HAVE_LIBEXECINFO )
			set( ALL_C_FLAGS "${ALL_C_FLAGS} -lexecinfo" )
		else( HAVE_LIBEXECINFO )
			set( HAVE_VM_JIT NO )
		endif( HAVE_LIBEXECINFO )
		set( CMAKE_REQUIRED_FLAGS )
	endif( NOT HAVE_BACKTRACE )
endif( HAVE_VM_JIT AND UNIX )

if( ${HAVE_VM_JIT} )
	if( ASMJIT_FOUND AND NOT FORCE_INTERNAL_ASMJIT )
		message( STATUS "Using system asmjit, includes found at ${ASMJIT_INCLUDE_DIR}" )
	else()
		message( STATUS "Using internal asmjit" )
		set( SKIP_INSTALL_ALL TRUE ) # Avoid installing asmjit alongside zdoom
		add_subdirectory( libraries/asmjit )
		set( ASMJIT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/asmjit )
		set( ASMJIT_LIBRARIES asmjit )
		set( ASMJIT_LIBRARY asmjit )
	endif()
endif()

if( JPEG_FOUND AND NOT FORCE_INTERNAL_JPEG )
	message( STATUS "Using system jpeg library, includes found at ${JPEG_INCLUDE_DIR}" )
else()
	message( STATUS "Using internal jpeg library" )
	add_subdirectory( libraries/jpeg )
	set( JPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/jpeg )
	set( JPEG_LIBRARIES jpeg )
	set( JPEG_LIBRARY jpeg )
endif()

if( BZIP2_FOUND AND NOT FORCE_INTERNAL_BZIP2 )
	message( STATUS "Using system bzip2 library, includes found at ${BZIP2_INCLUDE_DIR}" )
else()
	message( STATUS "Using internal bzip2 library" )
	add_subdirectory( libraries/bzip2 )
	set( BZIP2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libraries/bzip2" )
	set( BZIP2_LIBRARIES bz2 )
	set( BZIP2_LIBRARY bz2 )
endif()


set( LZMA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libraries/lzma/C" )

if( NOT CMAKE_CROSSCOMPILING )
	if( NOT CROSS_EXPORTS )
		set( CROSS_EXPORTS "" )
	endif()
endif()

# Install the entire docs directory in the distributed zip package
if( WIN32 )
	set( INSTALL_DOCS_PATH docs CACHE STRING "Directory where the documentation will be placed during install." )
else()
	set( INSTALL_DOCS_PATH share/doc/${ZDOOM_EXE_NAME} CACHE STRING "Directory where the zdoom documentation will be placed during install." )
endif()
install(DIRECTORY docs/
		DESTINATION ${INSTALL_DOCS_PATH}
		COMPONENT "Documentation")

option( DYN_OPENAL "Dynamically load OpenAL" ON )

add_subdirectory( libraries/lzma )
add_subdirectory( tools )
add_subdirectory( libraries/gdtoa )
add_subdirectory( wadsrc )
add_subdirectory( wadsrc_bm )
add_subdirectory( wadsrc_lights )
add_subdirectory( wadsrc_extra )
add_subdirectory( wadsrc_widescreen )
add_subdirectory( src )

if( NOT CMAKE_CROSSCOMPILING )
	export(TARGETS ${CROSS_EXPORTS} FILE "${CMAKE_BINARY_DIR}/ImportExecutables.cmake" )
endif()
