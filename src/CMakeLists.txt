cmake_minimum_required( VERSION 3.1.0 )

include(precompiled_headers)

if( COMMAND cmake_policy )
	cmake_policy( SET CMP0003 NEW )
endif()

include( CheckCXXSourceCompiles )
include( CheckFunctionExists )
include( CheckCXXCompilerFlag )
include( CheckIncludeFile )
include( CheckIncludeFiles )
include( CheckLibraryExists )
include( FindPkgConfig )

if( MSVC )
	find_package( ZMusic )
else()
	find_package( ZMusic REQUIRED )
endif()

message("Building for target architecture: ${ZDOOM_TARGET_ARCH}")

if( MSVC AND NOT ZMUSIC_FOUND )
	# Use prebuilt library
	set( ZMUSIC_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../bin/windows/zmusic" )
	set( ZMUSIC_INCLUDE_DIR ${ZMUSIC_ROOT_PATH}/include )
	set( ZMUSIC_LIBRARIES zmusic )
	if( ${ZDOOM_TARGET_ARCH} MATCHES "x86_64" )
		link_directories( ${ZMUSIC_ROOT_PATH}/64bit )
	elseif( ${ZDOOM_TARGET_ARCH} MATCHES "arm" )
		link_directories( ${ZMUSIC_ROOT_PATH}/arm64 )
	else()
		link_directories( ${ZMUSIC_ROOT_PATH}/32bit )
	endif()
	set( ZMUSIC_FOUND TRUE )
endif()

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	option( NO_STRIP "Do not strip Release or MinSizeRel builds" )
	# At least some versions of Xcode fail if you strip with the linker
	# instead of the separate strip utility.
	if( APPLE )
		set( NO_STRIP ON )
	endif()
endif()

if( APPLE )
    option( OSX_COCOA_BACKEND "Use native Cocoa backend instead of SDL" ON )
endif()

if( ${ZDOOM_TARGET_ARCH} MATCHES "x86_64" )
	set( X64 64 )
endif()

if( X64 OR ${ZDOOM_TARGET_ARCH} MATCHES "i386" )
	add_definitions( -DARCH_IA32 )
endif()

if( NOT ZDOOM_LIBS )
	set( ZDOOM_LIBS "" )
endif()

if( WIN32 )
	if( X64 )
		set( WIN_TYPE Win64 )
		set( XBITS x64 )
	else()
		set( WIN_TYPE Win32 )
		set( XBITS x86 )
	endif()

	add_definitions( -D_WIN32 )


	if( MSVC )	# For VS 2017 and later.
		# for modern Windows SDKs the DirectX headers should be available by default.
		set( DX_dinput8_LIBRARY dinput8 )
	else()

		find_library( DX_dinput8_LIBRARY dinput8
			PATHS ENV DXSDK_DIR
			PATH_SUFFIXES Lib Lib/${XBITS} )
		find_library( DX_dxguid_LIBRARY dxguid
			PATHS ENV DXSDK_DIR
			PATH_SUFFIXES Lib Lib/${XBITS} )

		# Modern versions of the Windows SDK include dinput8.lib. Unfortunately,
		# CMake cannot find these libraries via find_library.
		if( NOT DX_dinput8_LIBRARY )
			# If we got this far, assume dinput8.lib is in the system library path.
			set( DX_dinput8_LIBRARY dinput8 )
		endif()

		# Modern versions of the Windows SDK do NOT include dxguid.lib. Its contents
		# were moved to dinput8.lib.
		if( NOT DX_dxguid_LIBRARY )
			message( STATUS "Could not find dxguid.lib. Build may fail on old Windows SDKs.")
		endif()
	endif()

	set( ZDOOM_LIBS
		wsock32
		winmm
		"${DX_dinput8_LIBRARY}"
		ole32
		user32
		gdi32
		comctl32
		comdlg32
		ws2_32
		setupapi
		oleaut32
		dbghelp
		shlwapi )

	if( NOT ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		set( ZDOOM_LIBS ${ZDOOM_LIBS} DelayImp )
	endif()

	if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		if( DX_dxguid_LIBRARY )
			list( APPEND ZDOOM_LIBS "${DX_dxguid_LIBRARY}" )
		endif()
		list( APPEND ZDOOM_LIBS d3d9 )
	endif()
else()
	if( APPLE )
		set( NO_GTK ON )
		set( DYN_GTK OFF )

		# Prevent inclusion of fp.h and FixMath.h from Carbon framework
		# Declarations from these files are not used but cause the following conflicts:
		#  - redefinition of 'FixedToFloat' and 'FloatToFixed' macros
		#  - redefinition of 'pi' as different kind of symbol
		add_definitions( -D__FP__ -D__FIXMATH__ )
	else()
		option( NO_GTK "Disable GTK+ dialogs (Not applicable to Windows)" )
		option( DYN_GTK "Load GTK+ at runtime instead of compile time" ON )

		# Use GTK+ for the IWAD picker, if available.
		if( NOT NO_GTK )
			pkg_check_modules( GTK3 gtk+-3.0 )
			if( GTK3_FOUND )
				if( NOT DYN_GTK )
					set( ZDOOM_LIBS ${ZDOOM_LIBS} ${GTK3_LIBRARIES} )
				endif()
				include_directories( ${GTK3_INCLUDE_DIRS} )
				link_directories( ${GTK3_LIBRARY_DIRS} )
			else()
				pkg_check_modules( GTK2 gtk+-2.0 )
				if( GTK2_FOUND )
					if( NOT DYN_GTK )
						set( ZDOOM_LIBS ${ZDOOM_LIBS} ${GTK2_LIBRARIES} )
					endif()
					include_directories( ${GTK2_INCLUDE_DIRS} )
					link_directories( ${GTK2_LIBRARY_DIRS} )
				else()
					set( NO_GTK ON )
				endif()
			endif()
		endif()
	endif()

	option( NO_SDL_JOYSTICK "Disable SDL joystick support (Not applicable to Windows)" OFF )
	if ( NO_SDL_JOYSTICK )
		add_definitions( -DNO_SDL_JOYSTICK=1 )
	endif ( NO_SDL_JOYSTICK )

	if( NO_GTK )
		add_definitions( -DNO_GTK )
	elseif( DYN_GTK )
		add_definitions( -DDYN_GTK=1 )
	else()
		add_definitions( -DDYN_GTK=0 )
	endif()

	# Non-Windows version also needs SDL except native OS X backend
	if( NOT APPLE OR NOT OSX_COCOA_BACKEND )
		find_package( SDL2 REQUIRED )
		include_directories( "${SDL2_INCLUDE_DIR}" )
		set( ZDOOM_LIBS ${ZDOOM_LIBS} "${SDL2_LIBRARY}" )
	endif()

	find_path( FPU_CONTROL_DIR fpu_control.h )
	if( FPU_CONTROL_DIR )
		include_directories( ${FPU_CONTROL_DIR} )
		add_definitions( -DHAVE_FPU_CONTROL )
	endif()
endif()

if( NOT NO_OPENAL )
	if ( NOT DYN_OPENAL )	# DYN_OPENAL uses local copies of the headers.
		find_package( OpenAL )
		mark_as_advanced(CLEAR OPENAL_INCLUDE_DIR)
		if( OPENAL_INCLUDE_DIR )
			include_directories( ${OPENAL_INCLUDE_DIR} )
			mark_as_advanced(CLEAR OPENAL_LIBRARY)
			if( OPENAL_LIBRARY )
				set( ZDOOM_LIBS ${OPENAL_LIBRARY} ${ZDOOM_LIBS} )
			else()
				set( NO_OPENAL ON )
			endif()
		else()
			set( NO_OPENAL ON )
		endif()
	else()
		add_definitions( -DDYN_OPENAL )
	endif()
endif()

if( NO_OPENAL )
	add_definitions( -DNO_OPENAL=1 )
endif()

# Decide on SSE setup

# SSE only matters on 32-bit targets. We check compiler flags to know if we can do it.
if( CMAKE_SIZEOF_VOID_P MATCHES "4" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES ppc )
		CHECK_CXX_COMPILER_FLAG( "-msse2 -mfpmath=sse" CAN_DO_MFPMATH )
		CHECK_CXX_COMPILER_FLAG( -arch:SSE2 CAN_DO_ARCHSSE2 )
		if( CAN_DO_MFPMATH )
			set( SSE1_ENABLE "-msse -mfpmath=sse" )
			set( SSE2_ENABLE "-msse2 -mfpmath=sse" )
		elseif( CAN_DO_ARCHSSE2 )
			set( SSE1_ENABLE -arch:SSE )
			set( SSE2_ENABLE -arch:SSE2 )
		endif()
endif()

if( X64 )
	set( HAVE_MMX 1 )
else( X64 )
	set( SAFE_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )

	if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmmx")
	endif( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )

	CHECK_CXX_SOURCE_COMPILES("#include <mmintrin.h>
		int main(void) { __m64 v = _m_from_int(0); }"
		HAVE_MMX)

	set( CMAKE_CXX_FLAGS ${SAFE_CMAKE_CXX_FLAGS} )
endif( X64 )

CHECK_CXX_SOURCE_COMPILES("#include <ppl.h>
	int main() { concurrency::parallel_for(0, 1, 1, [](int) { } ); }"
	HAVE_PARALLEL_FOR)

if( NOT HAVE_PARALLEL_FOR )
	CHECK_CXX_SOURCE_COMPILES("#include <dispatch/dispatch.h>
		int main() { dispatch_apply(1, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t) { }); }"
		HAVE_DISPATCH_APPLY)
endif()

# Set up flags for MSVC
if (MSVC)
	set( CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}" )
endif (MSVC)

# Set up flags for GCC

if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	if( PROFILE )
		set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg" )
		set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg" )
		set( CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -pg" )
		set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg" )
	endif()

	#set( REL_CXX_FLAGS "-fno-rtti" )
	if( NOT PROFILE AND NOT APPLE )
		# On OS X frame pointers are required for exception handling, at least with Clang
		set( REL_CXX_FLAGS "${REL_CXX_FLAGS} -fomit-frame-pointer" )
	endif()
	set( CMAKE_CXX_FLAGS_RELEASE "${REL_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}" )
	set( CMAKE_CXX_FLAGS_MINSIZEREL "${REL_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}" )
	set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "${REL_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" )

	# Support for the GCC/Clang sanitizers.
	set( WITH_ASAN 0 CACHE BOOL "Enable the Address Sanitizer")
	if( NOT CMAKE_COMPILER_IS_GNUCXX )
		set( WITH_MSAN 0 CACHE BOOL "Enable the Memory Sanitizer")
	endif( NOT CMAKE_COMPILER_IS_GNUCXX )
	set( WITH_UBSAN 0 CACHE BOOL "Enable the Undefined Behavior Sanitizer")
	if( WITH_MSAN )
		if ( WITH_ASAN OR WITH_UBSAN )
			message( SEND_ERROR "You can't use MSAN with either ASAN or UBSAN." )
		endif ( WITH_ASAN OR WITH_UBSAN )
	endif( WITH_MSAN )

	set( SANITIZER_FLAG "" )
	if( WITH_ASAN )
		set( SANITIZER_FLAG "-fsanitize=address" )
		if ( WITH_UBSAN )
			set( SANITIZER_FLAG "${SANITIZER_FLAG},undefined" )
		endif( WITH_UBSAN )
	elseif( WITH_MSAN )
		set( SANITIZER_FLAG "-fsanitize=memory" )
	elseif( WITH_UBSAN )
		set( SANITIZER_FLAG "-fsanitize=undefined" )
	endif( WITH_ASAN )

	set( CMAKE_CXX_FLAGS "${SANITIZER_FLAG} ${CMAKE_CXX_FLAGS}" )
	set( CMAKE_C_FLAGS "${SANITIZER_FLAG} ${CMAKE_C_FLAGS}" )
	set( CMAKE_EXE_LINKER_FLAGS "${SANITIZER_FLAG} ${CMAKE_EXE_LINKER_FLAGS}" )

	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.5")
		set( CMAKE_C_FLAGS "-Wno-unused-result ${CMAKE_C_FLAGS}" )
		set( CMAKE_CXX_FLAGS "-Wno-unused-result ${CMAKE_CXX_FLAGS}" )
	endif()
	if( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
		if( APPLE OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "3.6" )
			set( CMAKE_CXX_FLAGS "-Wno-inconsistent-missing-override ${CMAKE_CXX_FLAGS}" )
		endif()
	endif()
	set( CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-missing-field-initializers -ffp-contract=off ${CMAKE_C_FLAGS}" )
	set( CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-missing-field-initializers -ffp-contract=off ${CMAKE_CXX_FLAGS}" )

	if( NOT X64 AND NOT CAN_DO_MFPMATH )
		set( CMAKE_C_FLAGS "-DNO_SSE ${CMAKE_C_FLAGS}" )
		set( CMAKE_CXX_FLAGS "-DNO_SSE ${CMAKE_CXX_FLAGS}" )
	endif()

	# Remove extra warnings when using the official DirectX headers.
	# Also, TDM-GCC 4.4.0 no longer accepts glibc-style printf formats as valid,
	# which is a royal pain. The previous version I had been using was fine with them.
	# MinGW: switch to the Windows Unicode API.
	if( WIN32 )
		set( CMAKE_CXX_FLAGS "-Wno-unknown-pragmas -Wno-comment -Wno-format ${CMAKE_CXX_FLAGS}" )
		set( CMAKE_CXX_FLAGS "-D_UNICODE -DUNICODE ${CMAKE_CXX_FLAGS}" )
		set( CMAKE_CXX_FLAGS "-D_WIN32_WINNT=0x0600 ${CMAKE_CXX_FLAGS}" )
		set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode" )
	endif()

	# Detect FreeBSD and add flags
	if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
		set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC" )
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC" )
	endif()
	
	if(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
		set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ljemalloc")
	endif()

	if( NOT NO_STRIP )
		set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s" )
		set (CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} -s" )
	endif()
endif()

# Check for thread_local keyword, it's optional at the moment

CHECK_CXX_SOURCE_COMPILES("thread_local int i; int main() { i = 0; }"
	HAVE_THREAD_LOCAL)

if( NOT HAVE_THREAD_LOCAL )
	message( SEND_ERROR "C++ compiler doesn't support thread_local storage duration specifier" )
endif()

# Check for functions that may or may not exist.

CHECK_FUNCTION_EXISTS( filelength FILELENGTH_EXISTS )
if( FILELENGTH_EXISTS )
	add_definitions( -DHAVE_FILELENGTH=1 )
endif()

CHECK_FUNCTION_EXISTS( strupr STRUPR_EXISTS )
if( NOT STRUPR_EXISTS )
	add_definitions( -DNEED_STRUPR=1 )
endif()

require_stricmp()
require_strnicmp()

if( NOT MSVC )
	add_definitions( -D__forceinline=inline )
endif()

if( UNIX )
	CHECK_LIBRARY_EXISTS( rt clock_gettime "" CLOCK_GETTIME_IN_RT )
	if( NOT CLOCK_GETTIME_IN_RT )
		CHECK_FUNCTION_EXISTS( clock_gettime CLOCK_GETTIME_EXISTS )
		if( NOT CLOCK_GETTIME_EXISTS )
			message( STATUS "Could not find clock_gettime. Timing statistics will not be available." )
			add_definitions( -DNO_CLOCK_GETTIME )
		endif()
	else()
		set( ZDOOM_LIBS ${ZDOOM_LIBS} rt )
	endif()
endif()

# Flags

# Update gitinfo.h

add_custom_target( revision_check ALL
	COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_SOURCE_DIR}/tools/updaterevision/UpdateRevision.cmake" src/gitinfo.h
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Libraries ZDoom needs

set( ZDOOM_LIBS ${ZDOOM_LIBS} "${ZLIB_LIBRARIES}" "${JPEG_LIBRARIES}" "${BZIP2_LIBRARIES}" "${CMAKE_DL_LIBS}" )
if (HAVE_VULKAN)
	set( ZDOOM_LIBS ${ZDOOM_LIBS} "glslang" "SPIRV" "OGLCompiler")
endif()
include_directories( "${ZLIB_INCLUDE_DIR}" "${BZIP2_INCLUDE_DIR}" "${LZMA_INCLUDE_DIR}" "${JPEG_INCLUDE_DIR}" "${ZMUSIC_INCLUDE_DIR}" )

if( ${HAVE_VM_JIT} )
	add_definitions( -DHAVE_VM_JIT )
	include_directories( "${ASMJIT_INCLUDE_DIR}" )
	set( ZDOOM_LIBS ${ZDOOM_LIBS} "${ASMJIT_LIBRARIES}")
endif()

# Start defining source files for ZDoom
set( PLAT_WIN32_SOURCES
	win32/i_steam.cpp
	common/platform/win32/hardware.cpp
	common/platform/win32/i_input.cpp
	common/platform/win32/i_keyboard.cpp
	common/platform/win32/i_mouse.cpp
	common/platform/win32/i_dijoy.cpp
	common/platform/win32/i_rawps2.cpp
	common/platform/win32/i_xinput.cpp
	common/platform/win32/i_main.cpp
	common/platform/win32/i_system.cpp
	common/platform/win32/i_specialpaths.cpp
	common/platform/win32/st_start.cpp
	common/platform/win32/st_start_util.cpp
	common/platform/win32/gl_sysfb.cpp
	common/platform/win32/base_sysfb.cpp
	common/platform/win32/win32basevideo.cpp
	common/platform/win32/win32glvideo.cpp
	common/platform/win32/win32polyvideo.cpp)

if (HAVE_VULKAN)
	set (PLAT_WIN32_SOURCES ${PLAT_WIN32_SOURCES} common/platform/win32/win32vulkanvideo.cpp )
endif()

# todo: implement an actual crash catcher for ARM
# for now this is purely experimental
if (NOT ${ZDOOM_TARGET_ARCH} MATCHES "arm" )
	set (PLAT_WIN32_SOURCES ${PLAT_WIN32_SOURCES} common/platform/win32/i_crash.cpp )
endif()
if (MSVC AND ${ZDOOM_TARGET_ARCH} MATCHES "arm")
	set (PLAT_WIN32_SOURCES ${PLAT_WIN32_SOURCES} common/platform/win32/i_crash_arm.cpp )
	add_definitions( -DNO_SSE -D__ARM__ -DRAPIDJSON_ENDIAN=RAPIDJSON_LITTLEENDIAN)
endif()
	
set( PLAT_POSIX_SOURCES
	posix/i_steam.cpp
	common/platform/posix/i_system_posix.cpp )
set( PLAT_SDL_SOURCES
	common/platform/posix/sdl/crashcatcher.c
	common/platform/posix/sdl/hardware.cpp
	common/platform/posix/sdl/i_gui.cpp
	common/platform/posix/sdl/i_input.cpp
	common/platform/posix/sdl/i_joystick.cpp
	common/platform/posix/sdl/i_main.cpp
	common/platform/posix/sdl/i_system.cpp
	common/platform/posix/sdl/sdlglvideo.cpp
	common/platform/posix/sdl/st_start.cpp )
set( PLAT_UNIX_SOURCES
	common/platform/posix/unix/i_specialpaths.cpp
	common/platform/posix/unix/gtk_dialogs.cpp )
set( PLAT_OSX_SOURCES
	common/platform/posix/osx/iwadpicker_cocoa.mm
	common/platform/posix/osx/i_specialpaths.mm
	posix/osx/zdoom.icns )
set( PLAT_COCOA_SOURCES
	common/platform/posix/cocoa/i_input.mm
	common/platform/posix/cocoa/i_joystick.cpp
	common/platform/posix/cocoa/i_main.mm
	common/platform/posix/cocoa/i_system.mm
	common/platform/posix/cocoa/i_video.mm
	common/platform/posix/cocoa/st_console.mm
	common/platform/posix/cocoa/st_start.mm )

if( WIN32 )
	set( SYSTEM_SOURCES_DIR common/platform/win32 )
	set( SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} )
	set( OTHER_SYSTEM_SOURCES ${PLAT_POSIX_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_OSX_SOURCES} ${PLAT_COCOA_SOURCES} ${PLAT_UNIX_SOURCES} )

	set( SYSTEM_SOURCES ${SYSTEM_SOURCES} win32/zdoom.rc )
elseif( APPLE )
	if( OSX_COCOA_BACKEND )
		set( SYSTEM_SOURCES_DIR common/platform/posix common/platform/posix/cocoa )
		set( SYSTEM_SOURCES ${PLAT_COCOA_SOURCES} )
		set( OTHER_SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_UNIX_SOURCES} )
	else()
		set( SYSTEM_SOURCES_DIR common/platform/posix common/platform/posix/sdl )
		set( SYSTEM_SOURCES ${PLAT_SDL_SOURCES} )
		set( PLAT_OSX_SOURCES ${PLAT_OSX_SOURCES} common/platform/posix/sdl/i_system.mm )
		set( OTHER_SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} ${PLAT_COCOA_SOURCES} ${PLAT_UNIX_SOURCES} )
	endif()

	set( SYSTEM_SOURCES ${SYSTEM_SOURCES} ${PLAT_POSIX_SOURCES} ${PLAT_OSX_SOURCES} )

	set_source_files_properties( posix/osx/zdoom.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
	set_source_files_properties( common/platform/posix/osx/iwadpicker_cocoa.mm PROPERTIES COMPILE_FLAGS -fobjc-exceptions )
else()
	set( SYSTEM_SOURCES_DIR common/platform/posix common/platform/posix/sdl )
	set( SYSTEM_SOURCES ${PLAT_POSIX_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_UNIX_SOURCES} )
	set( OTHER_SYSTEM_SOURCES ${PLAT_WIN32_SOURCES} ${PLAT_OSX_SOURCES} ${PLAT_COCOA_SOURCES} )
endif()

if( HAVE_MMX )
	add_definitions( -DHAVE_MMX=1 )

	set( SYSTEM_SOURCES ${SYSTEM_SOURCES}
		common/textures/hires/hqnx_asm/hq2x_asm.cpp
		common/textures/hires/hqnx_asm/hq3x_asm.cpp
		common/textures/hires/hqnx_asm/hq4x_asm.cpp
		common/textures/hires/hqnx_asm/hqnx_asm_Image.cpp)

	if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
		set_source_files_properties(
			common/textures/hires/hqnx_asm/hq2x_asm.cpp
			common/textures/hires/hqnx_asm/hq3x_asm.cpp
			common/textures/hires/hqnx_asm/hq4x_asm.cpp
			common/textures/hires/hqresize.cpp
			PROPERTIES COMPILE_FLAGS "-mmmx" )
	endif( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
endif( HAVE_MMX )

if( HAVE_PARALLEL_FOR )
	add_definitions( -DHAVE_PARALLEL_FOR=1 )
elseif( HAVE_DISPATCH_APPLY )
	add_definitions( -DHAVE_DISPATCH_APPLY=1 )
else()
	option( NO_OPENMP "Disable usage of OpenMP" OFF )

	if( NOT NO_OPENMP )
		include( FindOpenMP )

		if( OPENMP_FOUND )
			set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}" )
			set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}" )
		endif( OPENMP_FOUND )
	endif( NOT NO_OPENMP )
endif()

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.h
	COMMAND lemon -C${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/gamedata/xlat/xlat_parser.y
	DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/gamedata/xlat/xlat_parser.y )

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.c ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.h
	COMMAND lemon -C${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/frontend/zcc-parse.lemon
	DEPENDS lemon ${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/frontend/zcc-parse.lemon )

add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h
	COMMAND re2c --no-generation-date -s -o ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h ${CMAKE_CURRENT_SOURCE_DIR}/common/engine/sc_man_scanner.re
	DEPENDS re2c ${CMAKE_CURRENT_SOURCE_DIR}/common/engine/sc_man_scanner.re )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

option( SEND_ANON_STATS "Enable sending of anonymous hardware statistics" ON )

if( NOT SEND_ANON_STATS )
	add_definitions( -DNO_SEND_STATS )
endif()

# Project files should be aware of the header files. We can GLOB these since
# there's generally a new cpp for every header so this file will get changed
file( GLOB HEADER_FILES
	console/*.h
	playsim/*.h
	playsim/bots/*.h
	playsim/fragglescript/*.h
	playsim/mapthinkers/*.h
	g_statusbar/*.h
	gamedata/*.h
	gamedata/resourcefiles/*.h
	gamedata/fonts/*.h
	gamedata/xlat/*.h
	intermission/*.h
	maploader/*.h
	menu/*.h
	sound/*.h
	sound/backend/*.h*
	posix/*.h
	r_data/*.h
	common/audio/sound/thirdparty/*.h
	common/audio/sound/*.h
	common/audio/music/*.h*
	common/2d/*.h
	common/console/*.h
	common/utility/*.h
	common/engine/*.h
	common/menu/*.h
	common/statusbar/*.h
	common/fonts/*.h
	common/objects/*.h
	common/filesystem/*.h
	common/platform/posix/cocoa/*.h
	common/platform/posix/sdl/*.h
	common/platform/win32/*.h
	common/models/*.h
	common/textures/*.h
	common/textures/hires/hqnx/*.h
	common/textures/hires/hqnx_asm/*.h
	common/textures/hires/xbr/*.h
	common/thirdparty/*.h
	common/thirdparty/rapidjson/*.h
	common/thirdparty/math/*h
	common/rendering/*.h
	common/rendering/gl_load/*.h
	common/rendering/hwrenderer/data/*.h
	common/rendering/polyrenderer/*.h
	common/rendering/polyrenderer/math/*.h
	common/rendering/polyrenderer/drawers/*.h
	common/rendering/polyrenderer/backend/*.h
	common/rendering/vulkan/*.h
	common/rendering/vulkan/system/*.h
	common/rendering/vulkan/renderer/*.h
	common/rendering/vulkan/shaders/*.h
	common/rendering/vulkan/textures/*.h
	common/scripting/core/*h
	common/scripting/vm/*h
	common/scripting/jit/*h
	common/scripting/interface/*.h
	common/scripting/backend/*.h
	common/scripting/frontend/*.h
	utility/*.h
	scripting/*.h
	scripting/backend/*.h
	scripting/decorate/*.h
	scripting/zscript/*.h
	sound/midisources/*.h
	rendering/*.h
	rendering/2d/*.h
	rendering/swrenderer/*.h
	rendering/swrenderer/textures/*.h
	rendering/swrenderer/drawers/*.h
	rendering/swrenderer/scene/*.h
	rendering/swrenderer/segments/*.h
	rendering/swrenderer/line/*.h
	rendering/swrenderer/plane/*.h
	rendering/swrenderer/things/*.h
	rendering/swrenderer/viewport/*.h
	rendering/hwrenderer/*.h
	rendering/hwrenderer/scene/*.h
	*.h
)

set ( SWRENDER_SOURCES
	rendering/swrenderer/r_swcolormaps.cpp
	rendering/swrenderer/r_swrenderer.cpp
	rendering/swrenderer/r_renderthread.cpp
	rendering/swrenderer/drawers/r_draw.cpp
	rendering/swrenderer/drawers/r_draw_pal.cpp
	rendering/swrenderer/drawers/r_draw_rgba.cpp
	rendering/swrenderer/scene/r_3dfloors.cpp
	rendering/swrenderer/scene/r_light.cpp
	rendering/swrenderer/scene/r_opaque_pass.cpp
	rendering/swrenderer/scene/r_portal.cpp
	rendering/swrenderer/scene/r_scene.cpp
	rendering/swrenderer/scene/r_translucent_pass.cpp
	rendering/swrenderer/viewport/r_drawerargs.cpp
	rendering/swrenderer/viewport/r_skydrawer.cpp
	rendering/swrenderer/viewport/r_spandrawer.cpp
	rendering/swrenderer/viewport/r_spritedrawer.cpp
	rendering/swrenderer/viewport/r_viewport.cpp
	rendering/swrenderer/viewport/r_walldrawer.cpp
	rendering/swrenderer/line/r_line.cpp
	rendering/swrenderer/line/r_farclip_line.cpp
	rendering/swrenderer/line/r_walldraw.cpp
	rendering/swrenderer/line/r_wallsetup.cpp
	rendering/swrenderer/line/r_fogboundary.cpp
	rendering/swrenderer/line/r_renderdrawsegment.cpp
	rendering/swrenderer/segments/r_clipsegment.cpp
	rendering/swrenderer/segments/r_drawsegment.cpp
	rendering/swrenderer/segments/r_portalsegment.cpp
	rendering/swrenderer/things/r_visiblesprite.cpp
	rendering/swrenderer/things/r_visiblespritelist.cpp
	rendering/swrenderer/things/r_voxel.cpp
	rendering/swrenderer/things/r_particle.cpp
	rendering/swrenderer/things/r_playersprite.cpp
	rendering/swrenderer/things/r_sprite.cpp
	rendering/swrenderer/things/r_wallsprite.cpp
	rendering/swrenderer/things/r_decal.cpp
	rendering/swrenderer/things/r_model.cpp
	rendering/swrenderer/plane/r_visibleplane.cpp
	rendering/swrenderer/plane/r_visibleplanelist.cpp
	rendering/swrenderer/plane/r_skyplane.cpp
	rendering/swrenderer/plane/r_planerenderer.cpp
	rendering/swrenderer/plane/r_flatplane.cpp
	rendering/swrenderer/plane/r_slopeplane.cpp
)

set( POLYRENDER_SOURCES
	common/rendering/polyrenderer/drawers/poly_triangle.cpp
	common/rendering/polyrenderer/drawers/poly_thread.cpp
	common/rendering/polyrenderer/drawers/screen_triangle.cpp
	common/rendering/polyrenderer/drawers/screen_scanline_setup.cpp
	common/rendering/polyrenderer/drawers/screen_shader.cpp
	common/rendering/polyrenderer/drawers/screen_blend.cpp
)

# These files will be flagged as "headers" so that they appear in project files
# without being compiled.
set( NOT_COMPILED_SOURCE_FILES
	${OTHER_SYSTEM_SOURCES}
	${SWRENDER_SOURCES}
	${POLYRENDER_SOURCES}
	sc_man_scanner.h
	common/engine/sc_man_scanner.re
	g_statusbar/sbarinfo_commands.cpp
	gamedata/xlat/xlat_parser.y
	xlat_parser.c
	xlat_parser.h
	common/scripting/frontend/zcc-parse.lemon
	zcc-parse.c
	zcc-parse.h
	common/platform/win32/zutil.natvis
)

set( VM_JIT_SOURCES
	common/scripting/jit/jit.cpp
	common/scripting/jit/jit_runtime.cpp
	common/scripting/jit/jit_call.cpp
	common/scripting/jit/jit_flow.cpp
	common/scripting/jit/jit_load.cpp
	common/scripting/jit/jit_math.cpp
	common/scripting/jit/jit_move.cpp
	common/scripting/jit/jit_store.cpp
)

# This is disabled for now because I cannot find a way to give the .pch file a different name.
# Visual C++ 2015 seems hell-bent on only allowing one .pch file with the same name as the executable.
#enable_precompiled_headers( g_pch2.h FASTMATH_PCH_SOURCES )

# Enable fast math for some sources
set( FASTMATH_SOURCES
	rendering/swrenderer/r_all.cpp
	rendering/swrenderer/r_swscene.cpp
	common/rendering/polyrenderer/poly_all.cpp
	common/textures/hires/hqnx/init.cpp
	common/textures/hires/hqnx/hq2x.cpp
	common/textures/hires/hqnx/hq3x.cpp
	common/textures/hires/hqnx/hq4x.cpp
	common/textures/hires/xbr/xbrz.cpp
	common/textures/hires/xbr/xbrz_old.cpp
	common/rendering/gl_load/gl_load.c
	rendering/hwrenderer/hw_dynlightdata.cpp
	rendering/hwrenderer/scene/hw_bsp.cpp
	rendering/hwrenderer/scene/hw_fakeflat.cpp
	rendering/hwrenderer/scene/hw_decal.cpp
	rendering/hwrenderer/scene/hw_drawinfo.cpp
	rendering/hwrenderer/scene/hw_drawlist.cpp
	rendering/hwrenderer/scene/hw_clipper.cpp
	rendering/hwrenderer/scene/hw_flats.cpp
	rendering/hwrenderer/scene/hw_portal.cpp
	rendering/hwrenderer/scene/hw_renderhacks.cpp
	rendering/hwrenderer/scene/hw_sky.cpp
	rendering/hwrenderer/scene/hw_skyportal.cpp
	rendering/hwrenderer/scene/hw_sprites.cpp
	rendering/hwrenderer/scene/hw_spritelight.cpp
	rendering/hwrenderer/scene/hw_walls.cpp
	rendering/hwrenderer/scene/hw_walls_vertex.cpp
	rendering/hwrenderer/scene/hw_weapon.cpp
	common/utility/matrix.cpp
)
	
#Vulkan stuff must go into a separate list because it needs to be disabled for some platforms
set (VULKAN_SOURCES
	common/rendering/vulkan/system/vk_device.cpp
	common/rendering/vulkan/system/vk_swapchain.cpp
	common/rendering/vulkan/system/vk_builders.cpp
	common/rendering/vulkan/system/vk_framebuffer.cpp
	common/rendering/vulkan/system/vk_buffers.cpp
	common/rendering/vulkan/renderer/vk_renderstate.cpp
	common/rendering/vulkan/renderer/vk_renderpass.cpp
	common/rendering/vulkan/renderer/vk_streambuffer.cpp
	common/rendering/vulkan/renderer/vk_postprocess.cpp
	common/rendering/vulkan/renderer/vk_renderbuffers.cpp
	common/rendering/vulkan/shaders/vk_shader.cpp
	common/rendering/vulkan/textures/vk_samplers.cpp
	common/rendering/vulkan/textures/vk_hwtexture.cpp
	common/rendering/vulkan/textures/vk_imagetransition.cpp
	common/rendering/vulkan/thirdparty/volk/volk.c
	common/rendering/vulkan/thirdparty/vk_mem_alloc/vk_mem_alloc.cpp
)

if (HAVE_VULKAN)
	set (FASTMATH_SOURCES ${FASTMATH_SOURCES} ${VULKAN_SOURCES})
endif()

set (POLYBACKEND_SOURCES
	common/rendering/polyrenderer/backend/poly_framebuffer.cpp
	common/rendering/polyrenderer/backend/poly_buffers.cpp
	common/rendering/polyrenderer/backend/poly_hwtexture.cpp
	common/rendering/polyrenderer/backend/poly_renderstate.cpp
)
set (FASTMATH_SOURCES ${FASTMATH_SOURCES} ${POLYBACKEND_SOURCES})

set (PCH_SOURCES
	am_map.cpp
	playsim/bots/b_bot.cpp
	playsim/bots/b_func.cpp
	playsim/bots/b_game.cpp
	playsim/bots/b_move.cpp
	playsim/bots/b_think.cpp
	bbannouncer.cpp
	console/c_cmds.cpp
	console/c_notifybuffer.cpp
	console/c_functions.cpp
	ct_chat.cpp
	d_iwad.cpp
	d_main.cpp
	d_anonstats.cpp
	d_net.cpp
	d_netinfo.cpp
	d_protocol.cpp
	doomstat.cpp
	g_cvars.cpp
	g_dumpinfo.cpp
	g_game.cpp
	g_hub.cpp
	g_level.cpp
	gameconfigfile.cpp
	gitinfo.cpp
	hu_scores.cpp
	m_cheat.cpp
	m_misc.cpp
	playsim/p_acs.cpp
	playsim/p_actionfunctions.cpp
	p_conversation.cpp
	playsim/p_destructible.cpp
	playsim/p_effect.cpp
	playsim/p_enemy.cpp
	playsim/p_interaction.cpp
	playsim/p_lnspec.cpp
	playsim/p_map.cpp
	playsim/p_maputl.cpp
	playsim/p_mobj.cpp
	p_openmap.cpp
	playsim/p_pspr.cpp
	p_saveg.cpp
	p_setup.cpp
	playsim/p_spec.cpp
	p_states.cpp
	playsim/p_things.cpp
	p_tick.cpp
	playsim/p_user.cpp
	rendering/r_utility.cpp
	rendering/r_sky.cpp
	sound/s_advsound.cpp
	sound/s_sndseq.cpp
	sound/s_doomsound.cpp
	serializer_doom.cpp
	scriptutil.cpp
	st_stuff.cpp
	r_data/v_palette.cpp
	wi_stuff.cpp
	gamedata/a_keys.cpp
	gamedata/a_weapons.cpp
	gamedata/decallib.cpp
	gamedata/g_mapinfo.cpp
	gamedata/g_skill.cpp
	gamedata/gi.cpp
	gamedata/umapinfo.cpp
	gamedata/d_dehacked.cpp
	gamedata/g_doomedmap.cpp
	gamedata/info.cpp
	gamedata/keysections.cpp
	gamedata/p_terrain.cpp
	gamedata/statistics.cpp
	gamedata/teaminfo.cpp
	playsim/mapthinkers/a_decalfx.cpp
	playsim/mapthinkers/a_doors.cpp
	playsim/mapthinkers/a_lightning.cpp
	playsim/mapthinkers/a_quake.cpp
	playsim/mapthinkers/a_ceiling.cpp
	playsim/mapthinkers/a_floor.cpp
	playsim/mapthinkers/a_lights.cpp
	playsim/mapthinkers/a_lighttransfer.cpp
	playsim/mapthinkers/a_pillar.cpp
	playsim/mapthinkers/a_plats.cpp
	playsim/mapthinkers/a_pusher.cpp
	playsim/mapthinkers/a_scroll.cpp
	playsim/mapthinkers/dsectoreffect.cpp
	playsim/a_pickups.cpp
	playsim/a_action.cpp
	playsim/a_decals.cpp
	playsim/a_dynlight.cpp
	playsim/a_flashfader.cpp
	playsim/a_morph.cpp
	playsim/a_specialspot.cpp
	playsim/p_secnodes.cpp
	playsim/p_sectors.cpp
	playsim/p_sight.cpp
	playsim/p_switch.cpp
	playsim/p_tags.cpp
	playsim/p_teleport.cpp
	playsim/actorptrselect.cpp
	playsim/dthinker.cpp
	playsim/p_3dfloors.cpp
	playsim/p_3dmidtex.cpp
	playsim/p_linkedsectors.cpp
	playsim/p_trace.cpp
	playsim/po_man.cpp
	playsim/portal.cpp
	g_statusbar/hudmessages.cpp
	g_statusbar/shared_hud.cpp
	g_statusbar/sbarinfo.cpp
	g_statusbar/sbar_mugshot.cpp
	g_statusbar/shared_sbar.cpp
	rendering/2d/f_wipe.cpp
	rendering/2d/v_blend.cpp
	rendering/hwrenderer/hw_entrypoint.cpp
	rendering/hwrenderer/hw_vertexbuilder.cpp
	rendering/hwrenderer/doom_aabbtree.cpp
	rendering/hwrenderer/hw_models.cpp
	rendering/hwrenderer/hw_postprocessshader.cpp
	rendering/hwrenderer/hw_precache.cpp
	rendering/hwrenderer/scene/hw_lighting.cpp
	rendering/hwrenderer/scene/hw_drawlistadd.cpp
	rendering/hwrenderer/scene/hw_setcolor.cpp
	maploader/edata.cpp
	maploader/specials.cpp
	maploader/maploader.cpp
	maploader/slopes.cpp
	maploader/glnodes.cpp
	maploader/udmf.cpp
	maploader/usdf.cpp
	maploader/strifedialogue.cpp
	maploader/polyobjects.cpp
	maploader/renderinfo.cpp
	maploader/compatibility.cpp
	maploader/postprocessor.cpp
	menu/doommenu.cpp
	menu/loadsavemenu.cpp
	menu/playermenu.cpp
	gamedata/textures/animations.cpp
	gamedata/textures/anim_switches.cpp
	gamedata/textures/buildloader.cpp
	gamedata/p_xlat.cpp
	gamedata/xlat/parse_xlat.cpp
	gamedata/xlat/parsecontext.cpp
	playsim/fragglescript/t_func.cpp
	playsim/fragglescript/t_load.cpp
	playsim/fragglescript/t_oper.cpp
	playsim/fragglescript/t_parse.cpp
	playsim/fragglescript/t_prepro.cpp
	playsim/fragglescript/t_script.cpp
	playsim/fragglescript/t_spec.cpp
	playsim/fragglescript/t_variable.cpp
	playsim/fragglescript/t_cmd.cpp
	intermission/intermission.cpp
	intermission/intermission_parse.cpp
	r_data/colormaps.cpp
	r_data/gldefs.cpp
	r_data/a_dynlightdata.cpp
	r_data/r_translate.cpp
	r_data/sprites.cpp
	r_data/portalgroups.cpp
	r_data/voxeldef.cpp
	r_data/r_canvastexture.cpp
	r_data/r_interpolate.cpp
	r_data/r_vanillatrans.cpp
	r_data/r_sections.cpp
	r_data/models.cpp
	scripting/vmiterators.cpp
	scripting/vmthunks.cpp
	scripting/vmthunks_actors.cpp
	scripting/thingdef.cpp
	scripting/thingdef_data.cpp
	scripting/thingdef_properties.cpp
	scripting/backend/codegen_doom.cpp
	scripting/decorate/olddecorations.cpp
	scripting/decorate/thingdef_exp.cpp
	scripting/decorate/thingdef_parse.cpp
	scripting/decorate/thingdef_states.cpp
	scripting/zscript/zcc_compile_doom.cpp
	rendering/swrenderer/textures/r_swtexture.cpp
	rendering/swrenderer/textures/warptexture.cpp
	rendering/swrenderer/textures/swcanvastexture.cpp
	events.cpp
	common/audio/sound/i_sound.cpp
	common/audio/sound/oalsound.cpp
	common/audio/sound/s_environment.cpp
	common/audio/sound/s_sound.cpp
	common/audio/sound/s_reverbedit.cpp
	common/audio/music/music_midi_base.cpp
	common/audio/music/music.cpp
	common/audio/music/i_music.cpp
	common/audio/music/i_soundfont.cpp
	common/audio/music/music_config.cpp
	common/2d/v_2ddrawer.cpp
	common/2d/v_drawtext.cpp
	common/2d/v_draw.cpp
	common/thirdparty/gain_analysis.cpp
	common/thirdparty/sfmt/SFMT.cpp
	common/fonts/singlelumpfont.cpp
	common/fonts/singlepicfont.cpp
	common/fonts/specialfont.cpp
	common/fonts/font.cpp
	common/fonts/hexfont.cpp
	common/fonts/v_font.cpp
	common/fonts/v_text.cpp	
	common/textures/hw_ihwtexture.cpp
	common/textures/hw_material.cpp
	common/textures/bitmap.cpp
	common/textures/m_png.cpp
	common/textures/texture.cpp
	common/textures/gametexture.cpp
	common/textures/image.cpp
	common/textures/imagetexture.cpp
	common/textures/texturemanager.cpp
	common/textures/multipatchtexturebuilder.cpp
	common/textures/skyboxtexture.cpp
	common/textures/animtexture.cpp
	common/textures/v_collection.cpp
	common/textures/animlib.cpp
	common/textures/formats/automaptexture.cpp
	common/textures/formats/brightmaptexture.cpp
	common/textures/formats/buildtexture.cpp
	common/textures/formats/ddstexture.cpp
	common/textures/formats/flattexture.cpp
	common/textures/formats/fontchars.cpp
	common/textures/formats/imgztexture.cpp
	common/textures/formats/jpegtexture.cpp
	common/textures/formats/md5check.cpp
	common/textures/formats/multipatchtexture.cpp
	common/textures/formats/patchtexture.cpp
	common/textures/formats/pcxtexture.cpp
	common/textures/formats/pngtexture.cpp
	common/textures/formats/rawpagetexture.cpp
	common/textures/formats/emptytexture.cpp
	common/textures/formats/shadertexture.cpp
	common/textures/formats/tgatexture.cpp
	common/textures/formats/stbtexture.cpp
	common/textures/formats/anmtexture.cpp
	common/textures/hires/hqresize.cpp
	common/models/models_md3.cpp
	common/models/models_md2.cpp
	common/models/models_voxel.cpp
	common/models/models_ue1.cpp
	common/models/models_obj.cpp
	common/models/model.cpp
	common/models/voxels.cpp
	common/console/c_commandline.cpp
	common/console/c_buttons.cpp
	common/console/c_bind.cpp
	common/console/c_enginecmds.cpp
	common/console/c_consolebuffer.cpp
	common/console/c_cvars.cpp
	common/console/c_dispatch.cpp
	common/console/c_commandbuffer.cpp
	common/console/c_console.cpp
	common/console/c_notifybufferbase.cpp
	common/console/c_tabcomplete.cpp
	common/console/c_expr.cpp
	common/utility/engineerrors.cpp
	common/utility/i_module.cpp
	common/utility/m_alloc.cpp
	common/utility/utf8.cpp
	common/utility/palette.cpp
	common/utility/files.cpp
	common/utility/files_decompress.cpp
	common/utility/memarena.cpp
	common/utility/cmdlib.cpp
	common/utility/configfile.cpp
	common/utility/i_time.cpp
	common/utility/m_argv.cpp
	common/utility/s_playlist.cpp
	common/utility/zstrformat.cpp
	common/utility/name.cpp
	common/utility/r_memory.cpp
	common/thirdparty/base64.cpp
	common/thirdparty/md5.cpp
 	common/thirdparty/superfasthash.cpp
	common/filesystem/filesystem.cpp
	common/filesystem/ancientzip.cpp
	common/filesystem/file_7z.cpp
	common/filesystem/file_grp.cpp
	common/filesystem/file_lump.cpp
	common/filesystem/file_rff.cpp
	common/filesystem/file_wad.cpp
	common/filesystem/file_zip.cpp
	common/filesystem/file_pak.cpp
	common/filesystem/file_whres.cpp
	common/filesystem/file_ssi.cpp
	common/filesystem/file_directory.cpp
	common/filesystem/resourcefile.cpp
	common/engine/cycler.cpp
	common/engine/d_event.cpp
	common/engine/date.cpp
	common/engine/stats.cpp
	common/engine/sc_man.cpp
	common/engine/palettecontainer.cpp
	common/engine/stringtable.cpp
	common/engine/i_net.cpp
	common/engine/i_interface.cpp
	common/engine/renderstyle.cpp
	common/engine/v_colortables.cpp
	common/engine/serializer.cpp
	common/engine/m_joy.cpp
	common/engine/m_random.cpp
	common/objects/autosegs.cpp
	common/objects/dobject.cpp
	common/objects/dobjgc.cpp
	common/objects/dobjtype.cpp
	common/menu/joystickmenu.cpp
	common/menu/menu.cpp
	common/menu/messagebox.cpp
	common/menu/optionmenu.cpp
	common/menu/resolutionmenu.cpp
	common/menu/menudef.cpp
	common/menu/savegamemanager.cpp
	common/statusbar/base_sbar.cpp
	
	common/rendering/v_framebuffer.cpp
	common/rendering/v_video.cpp
	common/rendering/r_thread.cpp
	common/rendering/r_videoscale.cpp
	common/rendering/hwrenderer/hw_draw2d.cpp
	common/rendering/hwrenderer/data/hw_clock.cpp
	common/rendering/hwrenderer/data/hw_skydome.cpp
	common/rendering/hwrenderer/data/flatvertices.cpp
	common/rendering/hwrenderer/data/hw_viewpointbuffer.cpp
	common/rendering/hwrenderer/data/hw_modelvertexbuffer.cpp
	common/rendering/hwrenderer/data/hw_cvars.cpp
	common/rendering/hwrenderer/data/hw_vrmodes.cpp
	common/rendering/hwrenderer/data/hw_lightbuffer.cpp
	common/rendering/hwrenderer/data/hw_aabbtree.cpp
	common/rendering/hwrenderer/data/hw_shadowmap.cpp
	common/rendering/hwrenderer/data/hw_shaderpatcher.cpp
	common/rendering/hwrenderer/postprocessing/hw_postprocess.cpp
	common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp
	common/rendering/gl_load/gl_interface.cpp
	common/rendering/gl/gl_renderer.cpp
	common/rendering/gl/gl_stereo3d.cpp
	common/rendering/gl/gl_framebuffer.cpp
	common/rendering/gl/gl_renderstate.cpp
	common/rendering/gl/gl_renderbuffers.cpp
	common/rendering/gl/gl_postprocess.cpp
	common/rendering/gl/gl_postprocessstate.cpp
	common/rendering/gl/gl_debug.cpp
	common/rendering/gl/gl_buffers.cpp
	common/rendering/gl/gl_hwtexture.cpp
	common/rendering/gl/gl_samplers.cpp
	common/rendering/gl/gl_shader.cpp
	common/rendering/gl/gl_shaderprogram.cpp
	common/scripting/core/dictionary.cpp
	common/scripting/core/dynarrays.cpp
	common/scripting/core/symbols.cpp
	common/scripting/core/types.cpp
	common/scripting/core/scopebarrier.cpp
	common/scripting/core/vmdisasm.cpp
	common/scripting/core/imports.cpp
	common/scripting/vm/vmexec.cpp
	common/scripting/vm/vmframe.cpp
	common/scripting/interface/stringformat.cpp
	common/scripting/interface/vmnatives.cpp
	common/scripting/frontend/ast.cpp
	common/scripting/frontend/zcc_compile.cpp
	common/scripting/frontend/zcc_parser.cpp
	common/scripting/backend/vmbuilder.cpp
	common/scripting/backend/codegen.cpp
	
	utility/nodebuilder/nodebuild.cpp
	utility/nodebuilder/nodebuild_classify_nosse2.cpp
	utility/nodebuilder/nodebuild_events.cpp
	utility/nodebuilder/nodebuild_extract.cpp
	utility/nodebuilder/nodebuild_gl.cpp
	utility/nodebuilder/nodebuild_utility.cpp
)

if( ${HAVE_VM_JIT} )
	set( PCH_SOURCES ${PCH_SOURCES} ${VM_JIT_SOURCES} )
else()
	set( NOT_COMPILED_SOURCE_FILES ${NOT_COMPILED_SOURCE_FILES} ${VM_JIT_SOURCES} )
endif()

if( MSVC )
	enable_precompiled_headers( g_pch.h PCH_SOURCES )
else()
	# Temporary solution for compilers other than MSVC
	set_source_files_properties( ${PCH_SOURCES} PROPERTIES COMPILE_FLAGS "-include g_pch.h" )
endif()

add_executable( zdoom WIN32 MACOSX_BUNDLE
	${HEADER_FILES}
	${NOT_COMPILED_SOURCE_FILES}
	${SYSTEM_SOURCES}
	${FASTMATH_SOURCES}
	${PCH_SOURCES}
	common/utility/x86.cpp
	common/thirdparty/strnatcmp.c
	common/utility/zstring.cpp
	common/utility/findfile.cpp
	common/thirdparty/math/asin.c
	common/thirdparty/math/atan.c
	common/thirdparty/math/const.c
	common/thirdparty/math/cosh.c
	common/thirdparty/math/exp.c
	common/thirdparty/math/isnan.c
	common/thirdparty/math/log.c
	common/thirdparty/math/log10.c
	common/thirdparty/math/mtherr.c
	common/thirdparty/math/polevl.c
	common/thirdparty/math/pow.c
	common/thirdparty/math/powi.c
	common/thirdparty/math/sin.c
	common/thirdparty/math/sinh.c
	common/thirdparty/math/sqrt.c
	common/thirdparty/math/tan.c
	common/thirdparty/math/tanh.c
	common/thirdparty/math/fastsin.cpp
)

set_source_files_properties( ${FASTMATH_SOURCES} PROPERTIES COMPILE_FLAGS ${ZD_FASTMATH_FLAG} )
set_source_files_properties( xlat/parse_xlat.cpp PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c" )
set_source_files_properties( common/engine/sc_man.cpp PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h" )
set_source_files_properties( ${NOT_COMPILED_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE )


if(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
	# [BL] Solaris requires these to be explicitly linked.
	set( ZDOOM_LIBS ${ZDOOM_LIBS} nsl socket)
endif()

if( UNIX )
	find_package( Backtrace )
	if(Backtrace_FOUND)
		set( ZDOOM_LIBS ${ZDOOM_LIBS} ${Backtrace_LIBRARIES} )
	endif()
endif()

target_link_libraries( zdoom ${ZDOOM_LIBS} gdtoa lzma ${ZMUSIC_LIBRARIES} )

include_directories( .
	common/audio/sound
	common/audio/music
	common/2d
	common/thirdparty
	common/textures
	common/textures/formats
	common/textures/hires
	common/textures
	common/models
	common/filesystem
	common/utility
	common/console
	common/engine
	common/menu
	common/statusbar
	common/fonts
	common/objects
	common/rendering
	common/rendering/hwrenderer/data
	common/rendering/gl_load
	common/rendering/gl
	common/rendering/vulkan/thirdparty
	common/rendering/polyrenderer/backend
	common/rendering/polyrenderer/drawers
	common/scripting/vm
	common/scripting/jit
	common/scripting/core
	common/scripting/interface
	common/scripting/frontend
	common/scripting/backend
	g_statusbar
	console
	playsim
	playsim/bots
	playsim/mapthinkers
	gamedata
	gamedata/textures
	gamedata/fonts
	rendering
	rendering/hwrenderer
	rendering/2d
	r_data
	sound
	menu
	sound/backend
	xlat
	utility
	utility/nodebuilder
	scripting
	scripting/zscript
	rendering
	../libraries/gdtoa
	../libraries/glslang/glslang/Public
	../libraries/glslang/spirv
	${CMAKE_BINARY_DIR}/libraries/gdtoa
	${SYSTEM_SOURCES_DIR}
)

add_dependencies( zdoom revision_check )

# Due to some quirks, we need to do this in this order
if( NOT ZDOOM_OUTPUT_OLDSTYLE )
	# RUNTIME_OUTPUT_DIRECTORY does not exist in CMake 2.4.
	# Linux distributions are slow to adopt 2.6. :(
	set_target_properties( zdoom PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ZDOOM_OUTPUT_DIR} )
	set_target_properties( zdoom PROPERTIES OUTPUT_NAME ${ZDOOM_EXE_NAME} )
else()
	set_target_properties( zdoom PROPERTIES
		RUNTIME_OUTPUT_NAME ${ZDOOM_EXE_NAME}
		RUNTIME_OUTPUT_DIRECTORY_RELEASE ${ZDOOM_OUTPUT_DIR}
		RUNTIME_OUTPUT_NAME_DEBUG ${ZDOOM_EXE_NAME}d
		RUNTIME_OUTPUT_DIRECTORY_DEBUG ${ZDOOM_OUTPUT_DIR}
		RUNTIME_OUTPUT_NAME_MINSIZEREL ${ZDOOM_EXE_NAME}msr
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${ZDOOM_OUTPUT_DIR}
		RUNTIME_OUTPUT_NAME_RELWITHDEBINFO ${ZDOOM_EXE_NAME}rd
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${ZDOOM_OUTPUT_DIR}
	)
endif()

if( MSVC )
	option( ZDOOM_GENERATE_MAPFILE "Generate .map file for debugging." OFF )
	set( LINKERSTUFF "/MANIFEST:NO" )

	if( ZDOOM_GENERATE_MAPFILE )
		set( LINKERSTUFF "${LINKERSTUFF} /MAP" )
	endif()
	set_target_properties(zdoom PROPERTIES LINK_FLAGS ${LINKERSTUFF})

	add_custom_command(TARGET zdoom POST_BUILD
		COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}\\common\\platform\\win32\\manifest.xml\" -outputresource:\"$<TARGET_FILE:zdoom>\"\;\#1
		COMMENT "Adding manifest..."
	)

endif()

if( NOT WIN32 AND NOT APPLE )
	FILE( WRITE ${CMAKE_CURRENT_BINARY_DIR}/link-make "if [ ! -e ${ZDOOM_OUTPUT_DIR}/${ZDOOM_EXE_NAME} ]; then ln -sf ${CMAKE_CURRENT_BINARY_DIR}/${ZDOOM_EXE_NAME} ${ZDOOM_OUTPUT_DIR}/${ZDOOM_EXE_NAME}; fi" )
	add_custom_command( TARGET zdoom POST_BUILD
		COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/link-make
		COMMAND /bin/sh -c ${CMAKE_CURRENT_BINARY_DIR}/link-make )
endif()

add_custom_command(TARGET zdoom POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_SOURCE_DIR}/soundfont/gzdoom.sf2 $<TARGET_FILE_DIR:zdoom>/soundfonts/gzdoom.sf2
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_SOURCE_DIR}/fm_banks/GENMIDI.GS.wopl $<TARGET_FILE_DIR:zdoom>/fm_banks/GENMIDI.GS.wopl
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_SOURCE_DIR}/fm_banks/gs-by-papiezak-and-sneakernets.wopn $<TARGET_FILE_DIR:zdoom>/fm_banks/gs-by-papiezak-and-sneakernets.wopn
)

if( WIN32 )
	set( INSTALL_SOUNDFONT_PATH . CACHE STRING "Directory where soundfonts and WOPL/WOPN banks will be placed during install." )
else()
	set( INSTALL_SOUNDFONT_PATH share/games/doom CACHE STRING "Directory where soundfonts and WOPL/WOPN banks will be placed during install." )
endif()
install(DIRECTORY "${PROJECT_BINARY_DIR}/soundfonts" "${PROJECT_BINARY_DIR}/fm_banks"
		DESTINATION ${INSTALL_SOUNDFONT_PATH}
		COMPONENT "Soundfont resources")

if( CMAKE_COMPILER_IS_GNUCXX )
	# GCC misoptimizes this file
	set_source_files_properties( oplsynth/fmopl.cpp PROPERTIES COMPILE_FLAGS "-fno-tree-dominator-opts -fno-tree-fre" )
endif()
if( ZD_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
	# Need to enable intrinsics for these files.
	if( SSE_MATTERS )
		set_property( SOURCE
			common/rendering/polyrenderer/poly_all.cpp
			rendering/swrenderer/r_all.cpp
			utility/palette.cpp
			utility/x86.cpp
			APPEND_STRING PROPERTY COMPILE_FLAGS " -msse2 -mmmx" )
	endif()
endif()

if( APPLE )
	set( LINK_FRAMEWORKS "-framework Cocoa -framework IOKit -framework OpenGL")

	if( HAVE_VULKAN )
		set( LINK_FRAMEWORKS "${LINK_FRAMEWORKS} -framework QuartzCore" )
	endif()

	set_target_properties(zdoom PROPERTIES
		LINK_FLAGS "${LINK_FRAMEWORKS}"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/posix/osx/zdoom-info.plist"
		XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "" )

	# Dymanic libraries like libvulkan.dylib or libMoltenVK.dylib will be loaded by dlopen()
	# if placed in the directory with the main executable
	set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rpath @executable_path" )
endif()

if( WIN32 )
	set( INSTALL_PATH . CACHE STRING "Directory where the zdoom executable will be placed during install." )
else()
	set( INSTALL_PATH bin CACHE STRING "Directory where the zdoom executable will be placed during install." )
endif()
install(TARGETS zdoom
		DESTINATION ${INSTALL_PATH}
		COMPONENT "Game executable")

source_group("Audio Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/sound/.+")
source_group("Game Data" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gamedata/.+")
source_group("Game Data\\Fonts" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gamedata/fonts/.+")
source_group("Intermission" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/intermission/.+")
source_group("Map Loader" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/maploader/.+")
source_group("Menu" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/menu/.+")
source_group("Console" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/console/.+")
source_group("Playsim" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/playsim/.+")
source_group("Playsim\\Bots" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/playsim/bots/.+")
source_group("Playsim\\FraggleScript" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/playsim/fragglescript/.+")
source_group("Playsim\\Map Thinkers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/playsim/mapthinkers/.+")
source_group("Rendering" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/.+")
source_group("Rendering\\2D" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/2d/.+")
source_group("Rendering\\Hardware Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/gl/.+")
source_group("Rendering\\Hardware Renderer\\Scene" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/hwrenderer/scene/.+")
source_group("Rendering\\Software Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/.+")
source_group("Rendering\\Software Renderer\\Drawers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/drawers/.+")
source_group("Rendering\\Software Renderer\\Scene" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/scene/.+")
source_group("Rendering\\Software Renderer\\Segments" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/segments/.+")
source_group("Rendering\\Software Renderer\\Line" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/line/.+")
source_group("Rendering\\Software Renderer\\Plane" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/plane/.+")
source_group("Rendering\\Software Renderer\\Things" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/things/.+")
source_group("Rendering\\Software Renderer\\Viewport" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/rendering/swrenderer/viewport/.+")
source_group("Render Data" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/r_data/.+")
source_group("Render Interface" FILES r_defs.h r_renderer.h r_sky.cpp r_sky.h r_state.h r_utility.cpp r_utility.h)
source_group("Platforms\\POSIX Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/posix/.+")
source_group("Platforms\\Win32 Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/win32/.+")
source_group("Scripting\\Decorate frontend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/decorate/.+")
source_group("Scripting\\ZScript frontend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/zscript/.+")
source_group("Scripting\\Compiler backend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/backend/.+")
source_group("Scripting" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/scripting/.+")
source_group("Common" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/.+")
source_group("Common\\Audio" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/audio/.+")
source_group("Common\\Audio\\Sound" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/audio/sound/.+")
source_group("Common\\Audio\\Sound\\Third-party" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/audio/sound/thirdparty/.+")
source_group("Common\\Audio\\Music" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/audio/music.+")
source_group("Common\\Console" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/console/.+")
source_group("Common\\Utility" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/utility/.+")
source_group("Common\\Engine" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/engine/.+")
source_group("Common\\2D" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/2d/.+")
source_group("Common\\Objects" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/objects/.+")
source_group("Common\\Menu" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/menu/.+")
source_group("Common\\Fonts" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/fonts/.+")
source_group("Common\\File System" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/filesystem/.+")
source_group("Common\\Scripting" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/.+")
source_group("Common\\Scripting\\Interface" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/interface/.+")
source_group("Common\\Scripting\\Frontend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/frontend/.+" FILES ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.c ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.h)
source_group("Common\\Scripting\\Backend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/backend/.+")
source_group("Common\\Scripting\\Core" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/core/.+")
source_group("Common\\Scripting\\JIT" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/jit/.+")
source_group("Common\\Scripting\\VM" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/scripting/vm/.+")
source_group("Common\\Platforms\\Cocoa Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/platform/posix/cocoa/.+")
source_group("Common\\Platforms\\OS X Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/platform/posix/osx/.+")
source_group("Common\\Platforms\\Unix Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/platform/posix/unix/.+")
source_group("Common\\Platforms\\SDL Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/platform/posix/sdl/.+")
source_group("Common\\Platforms\\Win32 Files" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/platform/win32/.+")
source_group("Common\\Rendering" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/.+")
source_group("Common\\Rendering\\Hardware Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/hwrenderer/.+")
source_group("Common\\Rendering\\Hardware Renderer\\Data" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/hwrenderer/data/.+")
source_group("Common\\Rendering\\Hardware Renderer\\Postprocessing" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/hwrenderer/postprocessing/.+")
source_group("Common\\Rendering\\OpenGL Loader" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/gl_load/.+")
source_group("Common\\Rendering\\OpenGL Backend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/gl/.+")
source_group("Common\\Rendering\\Vulkan Renderer\\System" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/vulkan/system/.+")
source_group("Common\\Rendering\\Vulkan Renderer\\Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/vulkan/renderer/.+")
source_group("Common\\Rendering\\Vulkan Renderer\\Shaders" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/vulkan/shaders/.+")
source_group("Common\\Rendering\\Vulkan Renderer\\Textures" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/vulkan/textures/.+")
source_group("Common\\Rendering\\Vulkan Renderer\\Third Party" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/vulkan/thirdparty/.+")
source_group("Common\\Rendering\\Vulkan Renderer\\Third Party\\Volk" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/vulkan/thirdparty/volk/.+")
source_group("Common\\Rendering\\Vulkan Renderer\\Third Party\\Vk_Mem_Alloc" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/vulkan/thirdparty/vk_mem_alloc.+")
source_group("Common\\Rendering\\Poly Renderer" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/polyrenderer/.+")
source_group("Common\\Rendering\\Poly Renderer\\Drawers" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/polyrenderer/drawers/.+")
source_group("Common\\Rendering\\Poly Renderer\\Backend" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/rendering/polyrenderer/backend/.+")
source_group("Common\\Models" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/models/.+")
source_group("Common\\Textures" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/textures/.+")
source_group("Common\\Textures\\Hires" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/textures/hires/.+")
source_group("Common\\Textures\\Hires\\HQ Resize" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/textures/hires/hqnx/.+")
source_group("Common\\Textures\\Hires\\HQ Resize MMX version" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/textures/hires/hqnx_asm/.+")
source_group("Common\\Textures\\Hires\\XBRZ" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/textures/hires/xbr/.+")
source_group("Common\\Textures\\Formats" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/textures/formats/.+")
source_group("Common\\Third Party" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/thirdparty/.+")
source_group("Common\\Third Party\\Math" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/thirdparty/math/.+")
source_group("Common\\Third Party\\RapidJSON" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/thirdparty/rapidjson/.+")
source_group("Common\\Third Party\\SFMT" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/common/thirdparty/sfmt/.+")
source_group("Utility" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/utility/.+")
source_group("Utility\\Node Builder" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/utility/nodebuilder/.+")
source_group("Statusbar" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/g_statusbar/.+")
source_group("Versioning" FILES version.h win32/zdoom.rc)
source_group("Xlat" REGULAR_EXPRESSION "^${CMAKE_CURRENT_SOURCE_DIR}/xlat/.+" FILES ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.h)
source_group("Source Files" FILES ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h common/engine/sc_man_scanner.re)
