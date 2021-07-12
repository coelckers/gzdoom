cmake_minimum_required(VERSION 3.13...3.20)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(ReadVersion)
read_version(src/version.h EXTRACTED_VERSION)

project(GZDoom
	VERSION "${EXTRACTED_VERSION}"
	DESCRIPTION "Modder-friendly OpenGL and Vulkan source port based on the DOOM engine"
	HOMEPAGE_URL "https://zdoom.org/"
	LANGUAGES C CXX
)

include(AddPk3)
include(CMakePackageConfigHelpers)
include(TargetArch)
include(ZUtility)

# Fixup some GNUInstallDirs defaults
set(SAVED_PROJECT_NAME "${PROJECT_NAME}") # Fix default CMAKE_INSTALL_DOCDIR
string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME)
set(CMAKE_INSTALL_BINDIR games CACHE PATH "User executables (bin)")
set(CMAKE_INSTALL_DATADIR "share/${PROJECT_NAME}" CACHE PATH "Read-only architecture-independent data (share/${PROJECT_NAME})")
include(GNUInstallDirs)
set(PROJECT_NAME "${SAVED_PROJECT_NAME}")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING
		"Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
		FORCE)
endif()

if(MSVC AND CMAKE_VERSION VERSION_LESS 3.15)
	message(WARNING "Some things may be misconfigured. Please update to CMake >= 3.15 with Visual Studio.")
endif()
if(NOT CMAKE_MSVC_RUNTIME_LIBRARY)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install path prefix" FORCE)
endif()

target_architecture(ZDOOM_TARGET_ARCH)

string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWERCASE)
set(ZDOOM_OUTPUT_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Directory where ${PROJECT_NAME_LOWERCASE}.pk3 and the executable will be created.")
set(ZDOOM_EXE_NAME "${PROJECT_NAME_LOWERCASE}" CACHE FILEPATH "Name of the executable to create")
if(MSVC)
	# Allow the user to use ZDOOM_OUTPUT_DIR as a single release point.
	# Use zdoom, zdoomd, zdoom64, and zdoomd64 for the binary names
	option(ZDOOM_OUTPUT_OLDSTYLE "Don't use Release/Debug directories." OFF)
else()
	set(ZDOOM_OUTPUT_OLDSTYLE OFF)
endif()

# Support cross compiling
set(IMPORT_EXECUTABLES "IMPORTFILE-NOTFOUND" CACHE FILEPATH "Export file from native build (enables cross-compiling).")
if(IMPORT_EXECUTABLES)
	set(CMAKE_CROSSCOMPILING ON)
	include(${IMPORT_EXECUTABLES})
else()
	set(CROSS_EXPORTS "")
endif()

# Global compiler configuration
# WARNING: Target specific configuration should be preferred over adding things here
if(MSVC)
	#  optionally generate assembly output for checking crash locations.
	option(ZDOOM_GENERATE_ASM "Generate assembly output." OFF)
	if(ZDOOM_GENERATE_ASM)
		add_compile_options("/FAcs")
	endif()

	add_compile_options(
		"/GF" # String pooling
		"/Gy" # Function-level linking
		"/permissive-"
		"/Oy" "/Oi" "/GS-"

		# Disable warnings for unsecure CRT functions from VC8+
		"/wd4996"
	)

	add_link_options(
		"/opt:ref" # Eliminate unreferenced functions and data
		"/opt:icf" # Perform identical COMDAT folding
		"/nodefaultlib:msvcrt"
		$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:/TSAWARE>
		"/LARGEADDRESSAWARE"
	)

	# RelWithDebInfo uses /Ob1 by default instead of Ob2 like Release
	string(REPLACE "/Ob1 " "/Ob2 " CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
else()
	add_compile_options("-ffp-contract=off")

	if(APPLE)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")
		if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			# If we're compiling with a custom GCC on the Mac (which we know since g++-4.2 doesn't support C++11) statically link libgcc.
			add_compile_options("-static-libgcc")
		endif()
	endif()
endif()

# Install the entire docs directory in the distributed zip package
install(DIRECTORY docs/
	DESTINATION ${CMAKE_INSTALL_DOCDIR}
	COMPONENT "Documentation"
)

add_subdirectory(libraries)
add_subdirectory(tools)
add_subdirectory(wadsrc)
add_subdirectory(wadsrc_bm)
add_subdirectory(wadsrc_lights)
add_subdirectory(wadsrc_extra)
add_subdirectory(wadsrc_widescreen)
add_subdirectory(src)

if(NOT CMAKE_CROSSCOMPILING)
	export(TARGETS ${CROSS_EXPORTS} FILE "${CMAKE_BINARY_DIR}/ImportExecutables.cmake")
endif()

set(CPACK_PACKAGE_CONTACT "First Last <example@example.com>" CACHE STRING "Contact info for archive maintainer.")
string(TOLOWER "${PROJECT_NAME}" CPACK_PACKAGE_NAME)
# Use same prefix for packaging for consistency
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_STRIP_FILES ON)
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CPACK_PROJECT_CONFIG_FILE cmake/CPackProjectConfig.cmake)

if(WIN32)
	set(CPACK_GENERATOR "ZIP")
else()
	set(CPACK_GENERATOR "DEB")
	set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
	set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://zdoom.org")
	set(CPACK_DEBIAN_PACKAGE_SECTION "games")
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
	set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
	set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
endif()

include(CPack)
