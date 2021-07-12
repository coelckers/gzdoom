include(precompiled_headers)

include(CheckCXXSourceCompiles)
include(CheckFunctionExists)
include(CheckCXXCompilerFlag)
include(CheckLibraryExists)
include(FindPkgConfig)

# BEGIN: Generated sources

add_custom_target(revision_check ALL
	COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_SOURCE_DIR}/tools/updaterevision/UpdateRevision.cmake" ${CMAKE_CURRENT_BINARY_DIR}/gitinfo.h
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.c ${CMAKE_CURRENT_BINARY_DIR}/xlat_parser.h
	COMMAND lemon -C${CMAKE_CURRENT_BINARY_DIR} gamedata/xlat/xlat_parser.y
	DEPENDS gamedata/xlat/xlat_parser.y
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.c ${CMAKE_CURRENT_BINARY_DIR}/zcc-parse.h
	COMMAND lemon -C${CMAKE_CURRENT_BINARY_DIR} common/scripting/frontend/zcc-parse.lemon
	DEPENDS common/scripting/frontend/zcc-parse.lemon
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h
	COMMAND re2c --no-generation-date -s -o ${CMAKE_CURRENT_BINARY_DIR}/sc_man_scanner.h common/engine/sc_man_scanner.re
	DEPENDS common/engine/sc_man_scanner.re
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# END
# BEGIN: Basic target setup

add_executable(zdoom WIN32 MACOSX_BUNDLE)

set_target_properties(zdoom PROPERTIES
	MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/posix/osx/zdoom-info.plist"
	XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
)

if(NOT ZDOOM_OUTPUT_OLDSTYLE)
	set_target_properties(zdoom PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${ZDOOM_OUTPUT_DIR}
		OUTPUT_NAME ${ZDOOM_EXE_NAME}
	)
else()
	set_target_properties(zdoom PROPERTIES
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

option(SEND_ANON_STATS "Enable sending of anonymous hardware statistics" ON)

if(NOT SEND_ANON_STATS)
	target_compile_definitions(zdoom PRIVATE NO_SEND_STATS)
endif()

# END
# BEGIN: Operating system setup

message(STATUS "Building for target architecture: ${ZDOOM_TARGET_ARCH}")

if(ZDOOM_TARGET_ARCH MATCHES "x86_64")
	set(X64 64)
endif()

if(X64 OR ZDOOM_TARGET_ARCH MATCHES "i386")
	target_compile_definitions(zdoom PRIVATE ARCH_IA32)
endif()

if(WIN32)
	if(MSVC)
		target_link_libraries(zdoom PRIVATE DelayImp)
	else()
		if(X64)
			set(XBITS x64)
		else()
			set(XBITS x86)
		endif()

		find_library(DX_dxguid_LIBRARY dxguid
			PATHS ENV DXSDK_DIR
			PATH_SUFFIXES Lib Lib/${XBITS}
		)

		# Modern versions of the Windows SDK do NOT include dxguid.lib. Its contents
		# were moved to dinput8.lib.
		if(DX_dxguid_LIBRARY)
			target_link_libraries(zdoom PRIVATE "${DX_dxguid_LIBRARY}")
		endif()

		target_link_libraries(zdoom PRIVATE d3d9)
	endif()

	target_link_libraries(zdoom PRIVATE
		wsock32
		winmm
		ole32
		user32
		gdi32
		comctl32
		comdlg32
		ws2_32
		setupapi
		oleaut32
		dbghelp
		dinput8
	)

	target_compile_definitions(zdoom PRIVATE _UNICODE UNICODE _WIN32_WINNT=0x0600)
else()
	if(APPLE)
		set(NO_GTK ON)
		set(DYN_GTK OFF)

		option(OSX_COCOA_BACKEND "Use native Cocoa backend instead of SDL" ON)

		# Prevent inclusion of fp.h and FixMath.h from Carbon framework
		# Declarations from these files are not used but cause the following conflicts:
		#  - redefinition of 'FixedToFloat' and 'FloatToFixed' macros
		#  - redefinition of 'pi' as different kind of symbol
		target_compile_definitions(zdoom PRIVATE __FP__ __FIXMATH__)

		target_link_libraries(zdoom PRIVATE "-framework Cocoa" "-framework IOKit" "-framework OpenGL")

		if(HAVE_VULKAN)
			target_link_libraries(zdoom PRIVATE "-framework QuartzCore")
		endif()

		# Dymanic libraries like libvulkan.dylib or libMoltenVK.dylib will be loaded by dlopen()
		# if placed in the directory with the main executable
		set_target_properties(zdoom PROPERTIES
			BUILD_WITH_INSTALL_RPATH ON
			INSTALL_RPATH "@executable_path"
		)
	else()
		option(NO_GTK "Disable GTK+ dialogs (Not applicable to Windows)" OFF)
		option(DYN_GTK "Load GTK+ at runtime instead of compile time" ON)

		# Use GTK+ for the IWAD picker, if available.
		if(NOT NO_GTK)
			pkg_check_modules(GTK3 IMPORTED_TARGET gtk+-3.0)
			if(GTK3_FOUND)
				if(NOT DYN_GTK)
					target_link_libraries(zdoom PRIVATE PkgConfig::GTK3)
				else()
					target_include_directories(zdoom PRIVATE ${GTK3_INCLUDE_DIRS})
				endif()
			else()
				pkg_check_modules(GTK2 IMPORTED_TARGET gtk+-2.0)
				if(GTK2_FOUND)
					if(NOT DYN_GTK)
						target_link_libraries(zdoom PRIVATE PkgConfig::GTK2)
					else()
						target_include_directories(zdoom PRIVATE ${GTK2_INCLUDE_DIRS})
					endif()
				else()
					set(NO_GTK ON)
				endif()
			endif()
		endif()
	endif()

	if(NO_GTK)
		target_compile_definitions(zdoom PRIVATE NO_GTK)
	elseif(DYN_GTK)
		target_compile_definitions(zdoom PRIVATE DYN_GTK=1)
	else()
		target_compile_definitions(zdoom PRIVATE DYN_GTK=0)
	endif()

	option(NO_SDL_JOYSTICK "Disable SDL joystick support" OFF)
	if(NO_SDL_JOYSTICK)
		target_compile_definitions(zdoom PRIVATE NO_SDL_JOYSTICK=1)
	endif()

	# Non-Windows version also needs SDL except native OS X backend
	if(NOT APPLE OR NOT OSX_COCOA_BACKEND)
		find_package(SDL2 REQUIRED)
		target_include_directories(zdoom PRIVATE "${SDL2_INCLUDE_DIR}")
		target_link_libraries(zdoom PRIVATE "${SDL2_LIBRARY}")
	endif()

	find_path(FPU_CONTROL_DIR fpu_control.h)
	if(FPU_CONTROL_DIR)
		target_include_directories(zdoom PRIVATE ${FPU_CONTROL_DIR})
		target_compile_definitions(zdoom PRIVATE HAVE_FPU_CONTROL)
	endif()

	find_package(Backtrace)
	if(Backtrace_FOUND)
		target_link_libraries(zdoom PRIVATE ${Backtrace_LIBRARIES})
		target_include_directories(zdoom PRIVATE ${Backtrace_INCLUDE_DIRS})
	endif()

	if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
		# [BL] Solaris requires these to be explicitly linked.
		target_link_libraries(zdoom PRIVATE nsl socket)
	endif()
endif()

option(DYN_OPENAL "Dynamically load OpenAL" ON)
option(NO_OPENAL "Disable OpenAL sound support" OFF)
if(NOT NO_OPENAL)
	add_library(OpenAL INTERFACE)
	target_link_libraries(zdoom PRIVATE OpenAL)
	if(NOT DYN_OPENAL) # DYN_OPENAL uses local copies of the headers.
		find_package(OpenAL REQUIRED)
		mark_as_advanced(CLEAR OPENAL_INCLUDE_DIR OPENAL_LIBRARY)
		if(OPENAL_INCLUDE_DIR AND OPENAL_LIBRARY)
			target_include_directories(OpenAL INTERFACE ${OPENAL_INCLUDE_DIR})
			target_link_libraries(OpenAL INTERFACE ${OPENAL_LIBRARY})
		endif()
	else()
		target_compile_definitions(OpenAL INTERFACE DYN_OPENAL)
	endif()
else()
	target_compile_definitions(zdoom PRIVATE NO_OPENAL=1)
endif()

if(TARGET musl-fts)
	target_link_libraries(zdoom PRIVATE musl-fts)
endif()

# END
# BEGIN: Compiler setup

if(MSVC AND ZDOOM_TARGET_ARCH MATCHES "arm")
	target_compile_definitions(zdoom PRIVATE __ARM__ RAPIDJSON_ENDIAN=RAPIDJSON_LITTLEENDIAN)
endif()

# Instruction set extensions
if(X64)
	set(HAVE_MMX ON)
	set(HAVE_SSE2 ON)
elseif(ZDOOM_TARGET_ARCH MATCHES "i386")
	if(COMPILER_IS_GNUCXX_COMPATIBLE)
		set(MMX_ENABLE "-mmx")
		set(CMAKE_REQUIRED_FLAGS "${MMX_ENABLE}")
	endif()

	check_cxx_source_compiles(
		"#include <mmintrin.h>
		int main(void) { __m64 v = _m_from_int(0); }"
		HAVE_MMX
	)

	set(CMAKE_REQUIRED_FLAGS "")

	check_cxx_compiler_flag("-msse2" HAVE_SSE2)
	if(HAVE_SSE2)
		set(SSE2_ENABLE "-msse2")
	endif()
endif()

if(NOT HAVE_SSE2)
	target_compile_definitions(zdoom PRIVATE NO_SSE)
endif()

check_cxx_source_compiles(
	"#include <ppl.h>
	int main() { concurrency::parallel_for(0, 1, 1, [](int) { } ); }"
	HAVE_PARALLEL_FOR
)

if(NOT HAVE_PARALLEL_FOR)
	check_cxx_source_compiles(
		"#include <dispatch/dispatch.h>
		int main() { dispatch_apply(1, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t) { }); }"
		HAVE_DISPATCH_APPLY
	)
endif()

if(HAVE_PARALLEL_FOR)
	target_compile_definitions(zdoom PRIVATE HAVE_PARALLEL_FOR=1)
elseif(HAVE_DISPATCH_APPLY)
	target_compile_definitions(zdoom PRIVATE HAVE_DISPATCH_APPLY=1)
else()
	option(NO_OPENMP "Disable usage of OpenMP" OFF)

	if(NOT NO_OPENMP)
		find_package(OpenMP)
		if(OPENMP_FOUND)
			target_link_libraries(zdoom PRIVATE OpenMP::OpenMP_CXX)
		endif()
	endif()
endif()

# Set up flags for MSVC
if(MSVC)
	target_compile_options(zdoom PRIVATE "/MP")

	target_link_options(zdoom PRIVATE "/MANIFEST:NO")

	option(ZDOOM_GENERATE_MAPFILE "Generate .map file for debugging." OFF)
	if(ZDOOM_GENERATE_MAPFILE)
		target_link_options(zdoom PRIVATE "/MAP")
	endif()

	add_custom_command(TARGET zdoom POST_BUILD
		COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}\\common\\platform\\win32\\manifest.xml\" -outputresource:\"$<TARGET_FILE:zdoom>\"\;\#1
		COMMENT "Adding manifest..."
	)
endif()

# Set up flags for GCC
if(COMPILER_IS_GNUCXX_COMPATIBLE)
	target_compile_definitions(zdoom PRIVATE __forceinline=inline)

	set(ZDOOM_PROFILE FALSE CACHE BOOL "Enable profiling with gprof for Debug and RelWithDebInfo build types.")
	if(ZDOOM_PROFILE)
		target_compile_options(zdoom PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-pg>)
	else()
		if(NOT APPLE)
			# On OS X frame pointers are required for exception handling, at least with Clang
			target_compile_options(zdoom PRIVATE -fomit-frame-pointer)
		endif()
	endif()

	# Support for the GCC/Clang sanitizers.
	option(WITH_ASAN "Enable the Address Sanitizer" OFF)
	option(WITH_MSAN "Enable the Memory Sanitizer" OFF)
	option(WITH_UBSAN "Enable the Undefined Behavior Sanitizer" OFF)
	if(WITH_MSAN)
		if(WITH_ASAN OR WITH_UBSAN)
			message(SEND_ERROR "You can't use MSAN with either ASAN or UBSAN.")
		endif()
	endif()

	if(WITH_ASAN AND WITH_UBSAN)
		target_compile_options(zdoom PRIVATE "-fsanitize=address,undefined")
	elseif(WITH_ASAN)
		target_compile_options(zdoom PRIVATE "-fsanitize=address")
	elseif(WITH_UBSAN)
		target_compile_options(zdoom PRIVATE "-fsanitize=undefined")
	elseif(WITH_MSAN)
		target_compile_options(zdoom PRIVATE "-fsanitize=memory")
	endif()

	target_compile_options(zdoom PRIVATE -Wall -Wextra -Wno-unused -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-result -ffp-contract=off)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		target_compile_options(zdoom PRIVATE -Wno-inconsistent-missing-override)
	endif()
	# Remove extra warnings when using the official DirectX headers.
	# Also, TDM-GCC 4.4.0 no longer accepts glibc-style printf formats as valid,
	# which is a royal pain. The previous version I had been using was fine with them.
	# MinGW: switch to the Windows Unicode API.
	if(WIN32)
		target_compile_options(zdoom PRIVATE -Wno-unknown-pragmas -Wno-comment -Wno-format)
		target_link_options(zdoom PRIVATE -municode)
	endif()
	
	if(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
		target_link_libraries(zdoom PRIVATE jemalloc)
	endif()
endif()

# Check for thread_local keyword, it's optional at the moment

# For some reason check_cxx_source_compiles isn't respecting CMAKE_CXX_STANDARD
# like CMP0067 would imply it should. This is a bit of a hack since this
# variable isn't exactly for specifying options to the underlying try_compile.
set(CMAKE_REQUIRED_DEFINITIONS CXX_STANDARD ${CMAKE_CXX_STANDARD})
check_cxx_source_compiles("thread_local int i; int main() { i = 0; }" HAVE_THREAD_LOCAL)
set(CMAKE_REQUIRED_DEFINITIONS)
if(NOT HAVE_THREAD_LOCAL)
	message(SEND_ERROR "C++ compiler doesn't support thread_local storage duration specifier")
endif()

# Check for functions that may or may not exist.

require_stricmp(zdoom PRIVATE)
require_strnicmp(zdoom PRIVATE)

if(UNIX)
	check_library_exists(rt clock_gettime "" CLOCK_GETTIME_IN_RT)
	if(NOT CLOCK_GETTIME_IN_RT)
		check_function_exists(clock_gettime CLOCK_GETTIME_EXISTS)
		if(NOT CLOCK_GETTIME_EXISTS)
			message(STATUS "Could not find clock_gettime. Timing statistics will not be available.")
			add_definitions(-DNO_CLOCK_GETTIME)
		endif()
	else()
		target_link_libraries(zdoom PRIVATE rt)
	endif()
endif()

# END
# BEGIN: Source file listing

target_include_directories(zdoom PRIVATE . ${CMAKE_CURRENT_BINARY_DIR}
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
)

# Project files should be aware of the header files. We can GLOB these since
# there's generally a new cpp for every header so this file will get changed
file(GLOB HEADER_FILES
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
target_sources(zdoom PRIVATE ${HEADER_FILES})

# Start defining source files. NOTE that certain generators use the target
# source list to determine what files should be visibile from the project file
# so it's generally preferred to add sources and then mark them as "header"
# files if we don't want them compiled.
#
# Generally speaking a list should be named here only if it either requires
# specific compile flags, or there are conditions upon which the sources are
# disabled.

set(PLAT_WIN32_SOURCES
	win32/i_steam.cpp
	win32/zdoom.rc
	common/platform/win32/hardware.cpp
	common/platform/win32/i_crash.cpp
	common/platform/win32/i_crash_arm.cpp
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
	common/platform/win32/win32polyvideo.cpp
	common/platform/win32/win32vulkanvideo.cpp
)

set(PLAT_POSIX_SOURCES
	posix/i_steam.cpp
	common/platform/posix/i_system_posix.cpp
)
set(PLAT_SDL_SOURCES
	common/platform/posix/sdl/crashcatcher.c
	common/platform/posix/sdl/hardware.cpp
	common/platform/posix/sdl/i_gui.cpp
	common/platform/posix/sdl/i_input.cpp
	common/platform/posix/sdl/i_joystick.cpp
	common/platform/posix/sdl/i_main.cpp
	common/platform/posix/sdl/i_system.cpp
	common/platform/posix/sdl/sdlglvideo.cpp
	common/platform/posix/sdl/st_start.cpp
)
set(PLAT_UNIX_SOURCES
	common/platform/posix/unix/i_specialpaths.cpp
	common/platform/posix/unix/gtk_dialogs.cpp
)
set(PLAT_OSX_SOURCES
	common/platform/posix/osx/iwadpicker_cocoa.mm
	common/platform/posix/osx/i_specialpaths.mm
	posix/osx/zdoom.icns
)
set(PLAT_COCOA_SOURCES
	common/platform/posix/cocoa/i_input.mm
	common/platform/posix/cocoa/i_joystick.cpp
	common/platform/posix/cocoa/i_main.mm
	common/platform/posix/cocoa/i_system.mm
	common/platform/posix/cocoa/i_video.mm
	common/platform/posix/cocoa/st_console.mm
	common/platform/posix/cocoa/st_start.mm
)

if(WIN32)
	target_include_directories(zdoom PRIVATE common/platform/win32)
	target_sources(zdoom PRIVATE ${PLAT_WIN32_SOURCES})
	set(NOT_COMPILED_SOURCE_FILES ${PLAT_POSIX_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_OSX_SOURCES} ${PLAT_COCOA_SOURCES} ${PLAT_UNIX_SOURCES})
elseif(APPLE)
	target_include_directories(zdoom PRIVATE common/platform/posix)
	if(OSX_COCOA_BACKEND)
		target_include_directories(zdoom PRIVATE common/platform/posix/cocoa)
		target_sources(zdoom PRIVATE ${PLAT_COCOA_SOURCES})
		set(NOT_COMPILED_SOURCE_FILES ${PLAT_WIN32_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_UNIX_SOURCES})
	else()
		list(APPEND PLAT_OSX_SOURCES common/platform/posix/sdl/i_system.mm)
		target_include_directories(zdoom PRIVATE common/platform/posix common/platform/posix/sdl)
		target_sources(zdoom PRIVATE ${PLAT_SDL_SOURCES})
		set(NOT_COMPILED_SOURCE_FILES ${PLAT_WIN32_SOURCES} ${PLAT_COCOA_SOURCES} ${PLAT_UNIX_SOURCES})
	endif()

	target_sources(zdoom PRIVATE ${PLAT_POSIX_SOURCES} ${PLAT_OSX_SOURCES})
else()
	target_include_directories(zdoom PRIVATE common/platform/posix common/platform/posix/sdl)
	target_sources(zdoom PRIVATE ${PLAT_POSIX_SOURCES} ${PLAT_SDL_SOURCES} ${PLAT_UNIX_SOURCES})
	set(NOT_COMPILED_SOURCE_FILES ${PLAT_WIN32_SOURCES} ${PLAT_OSX_SOURCES} ${PLAT_COCOA_SOURCES})
endif()

set(MMX_SOURCES
	common/textures/hires/hqnx_asm/hq2x_asm.cpp
	common/textures/hires/hqnx_asm/hq3x_asm.cpp
	common/textures/hires/hqnx_asm/hq4x_asm.cpp
	common/textures/hires/hqnx_asm/hqnx_asm_Image.cpp
)
target_sources(zdoom PRIVATE ${MMX_SOURCES})

set(VM_JIT_SOURCES
	common/scripting/jit/jit.cpp
	common/scripting/jit/jit_runtime.cpp
	common/scripting/jit/jit_call.cpp
	common/scripting/jit/jit_flow.cpp
	common/scripting/jit/jit_load.cpp
	common/scripting/jit/jit_math.cpp
	common/scripting/jit/jit_move.cpp
	common/scripting/jit/jit_store.cpp
)
target_sources(zdoom PRIVATE ${VM_JIT_SOURCES})

# Software renderer sources
list(APPEND NOT_COMPILED_SOURCE_FILES
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

# Polyrender sources
list(APPEND NOT_COMPILED_SOURCE_FILES
	common/rendering/polyrenderer/drawers/poly_triangle.cpp
	common/rendering/polyrenderer/drawers/poly_thread.cpp
	common/rendering/polyrenderer/drawers/screen_triangle.cpp
	common/rendering/polyrenderer/drawers/screen_scanline_setup.cpp
	common/rendering/polyrenderer/drawers/screen_shader.cpp
	common/rendering/polyrenderer/drawers/screen_blend.cpp
)

# These files will be flagged as "headers" so that they appear in project files
# without being compiled.
list(APPEND NOT_COMPILED_SOURCE_FILES
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

# Enable fast math for some sources
set(FASTMATH_SOURCES
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

	# Polybackend sources
	common/rendering/polyrenderer/backend/poly_framebuffer.cpp
	common/rendering/polyrenderer/backend/poly_buffers.cpp
	common/rendering/polyrenderer/backend/poly_hwtexture.cpp
	common/rendering/polyrenderer/backend/poly_renderstate.cpp
)
	
# Vulkan stuff must go into a separate list because it needs to be disabled for some platforms
set(VULKAN_SOURCES
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
list(APPEND FASTMATH_SOURCES ${VULKAN_SOURCES})

set(PCH_SOURCES
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

target_sources(zdoom PRIVATE
	${NOT_COMPILED_SOURCE_FILES}
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

# END
# BEGIN: Source disablement

get_property(ZMUSIC_TYPE TARGET ZMusic::zmusic PROPERTY TYPE)
if(ZMUSIC_TYPE STREQUAL "STATIC_LIBRARY")
	# ZMusic has a copy of this translation unit
	list(REMOVE_ITEM PCH_SOURCES common/utility/i_module.cpp)
	list(APPEND NOT_COMPILED_SOURCE_FILES common/utility/i_module.cpp)
endif()

if(NOT HAVE_VM_JIT)
	list(APPEND NOT_COMPILED_SOURCE_FILES ${VM_JIT_SOURCES})
endif()

if(NOT HAVE_VULKAN)
	list(APPEND NOT_COMPILED_SOURCE_FILES
		${VULKAN_SOURCES}
		common/platform/win32/win32vulkanvideo.cpp
	)
endif()

if(ZDOOM_TARGET_ARCH MATCHES "arm")
	list(APPEND NOT_COMPILED_SOURCE_FILES common/platform/win32/i_crash.cpp)
else()
	list(APPEND NOT_COMPILED_SOURCE_FILES common/platform/win32/i_crash_arm.cpp)
endif()

if(NOT HAVE_MMX)
	list(APPEND NOT_COMPILED_SOURCE_FILES ${MXX_SOURCES})
endif()

# END
# BEGIN: Source properties and linking

if(MSVC)
	enable_precompiled_headers(zdoom g_pch.h PCH_SOURCES)
else()
	# Temporary solution for compilers other than MSVC
	set_source_files_properties(${PCH_SOURCES} PROPERTIES COMPILE_FLAGS "-include g_pch.h")
endif()

use_fast_math(${FASTMATH_SOURCES})
set_source_files_properties(${NOT_COMPILED_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

set_source_files_properties(posix/osx/zdoom.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties(common/platform/posix/osx/iwadpicker_cocoa.mm PROPERTIES COMPILE_FLAGS -fobjc-exceptions)

if(CMAKE_COMPILER_IS_GNUCXX)
	# GCC misoptimizes this file
	set_source_files_properties(oplsynth/fmopl.cpp PROPERTIES COMPILE_FLAGS "-fno-tree-dominator-opts -fno-tree-fre")
endif()

if(HAVE_MMX)
	target_compile_definitions(zdoom PRIVATE HAVE_MMX=1)

	if(MMX_ENABLE)
		set_property(SOURCE ${MMX_SOURCES} APPEND PROPERTY COMPILE_FLAGS "${MMX_ENABLE}")
	endif()
endif()

if(SSE2_ENABLE)
	# Need to enable intrinsics for these files.
	set_property(
		SOURCE
			common/rendering/polyrenderer/poly_all.cpp
			rendering/swrenderer/r_all.cpp
			common/utility/palette.cpp
			common/utility/x86.cpp
		APPEND PROPERTY COMPILE_FLAGS "${SSE2_ENABLE}"
	)
endif()

# Libraries ZDoom needs
target_link_libraries(zdoom PRIVATE ZLIB::ZLIB JPEG::JPEG BZip2::BZip2 ZMusic::zmusic gdtoa lzma ${CMAKE_DL_LIBS})

target_compile_definitions(zdoom PRIVATE HAVE_SOFTPOLY)

if (HAVE_VULKAN)
	target_compile_definitions(zdoom PRIVATE HAVE_VULKAN)
	target_link_libraries(zdoom PRIVATE glslang SPIRV OGLCompiler)
endif()

if(HAVE_VM_JIT)
	target_link_libraries(zdoom PRIVATE asmjit)
	target_compile_definitions(zdoom PRIVATE HAVE_VM_JIT)
endif()

# END
# BEGIN: Install rules

add_custom_command(TARGET zdoom POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_SOURCE_DIR}/soundfont/gzdoom.sf2 $<TARGET_FILE_DIR:zdoom>/soundfonts/gzdoom.sf2
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_SOURCE_DIR}/fm_banks/GENMIDI.GS.wopl $<TARGET_FILE_DIR:zdoom>/fm_banks/GENMIDI.GS.wopl
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	${CMAKE_SOURCE_DIR}/fm_banks/gs-by-papiezak-and-sneakernets.wopn $<TARGET_FILE_DIR:zdoom>/fm_banks/gs-by-papiezak-and-sneakernets.wopn
)

install(DIRECTORY "${PROJECT_BINARY_DIR}/soundfonts" "${PROJECT_BINARY_DIR}/fm_banks"
	DESTINATION ${CMAKE_INSTALL_DATADIR}
	COMPONENT "Soundfont resources"
)

install(TARGETS zdoom
	DESTINATION ${CMAKE_INSTALL_BINDIR}
	COMPONENT "Game executable"
)

# END
# BEGIN: Source groups

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

# END
