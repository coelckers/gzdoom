include_guard(DIRECTORY)

include(CheckFunctionExists)

# BEGIN: Variables

set(COMPILER_ID_GNU_COMPATIBLE "AppleClang;Clang;GNU")

# Replacement variables for a possible long list of C/C++ compilers compatible with GCC
if(CMAKE_C_COMPILER_ID IN_LIST COMPILER_ID_GNU_COMPATIBLE)
	set(COMPILER_IS_GNUC_COMPATIBLE TRUE)
else()
	set(COMPILER_IS_GNUC_COMPATIBLE FALSE)
endif()

if(CMAKE_CXX_COMPILER_ID IN_LIST COMPILER_ID_GNU_COMPATIBLE)
	set(COMPILER_IS_GNUCXX_COMPATIBLE TRUE)
else()
	set(COMPILER_IS_GNUCXX_COMPATIBLE FALSE)
endif()

# BEGIN: Functions

function(determine_package_config_dependency)
	# Don't need to worry about this when building shared libraries
	if(BUILD_SHARED_LIBS)
		return()
	endif()

	set(Dest "${ARGV0}")
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "TARGET;MODULE" "")

	if(TARGET ${ARG_TARGET})
		get_property(TgtImported TARGET ${ARG_TARGET} PROPERTY IMPORTED)
		if(TgtImported)
			list(APPEND "${Dest}" "${ARG_MODULE}")
		endif()
	endif()

	set("${Dest}" "${${Dest}}" CACHE INTERNAL "")
endfunction()

# Adds private links to a given target, but any OBJECT or INTERFACE library will
# be hidden from install(EXPORT).
function(target_link_libraries_hidden Tgt)
	set(Links ${ARGV})
	list(REMOVE_AT Links 0)

	get_property(TgtType TARGET "${Tgt}" PROPERTY TYPE)

	foreach(Link IN LISTS Links)
		if(TARGET "${Link}")
			get_property(LinkImported TARGET "${Link}" PROPERTY IMPORTED)
			get_property(LinkType TARGET "${Link}" PROPERTY TYPE)

			if(NOT LinkImported AND (LinkType STREQUAL "OBJECT_LIBRARY" OR LinkType STREQUAL "INTERFACE_LIBRARY"))
				target_link_libraries("${Tgt}" PRIVATE "$<BUILD_INTERFACE:${Link}>")

				# Since we're potentially hiding usage requirements from the
				# exported targets, we need to do this recursively.
				if(TgtType STREQUAL "STATIC_LIBRARY")
					get_property(TransitiveLinks TARGET "${Link}" PROPERTY INTERFACE_LINK_LIBRARIES)
					target_link_libraries_hidden("${Tgt}" ${TransitiveLinks})
				endif()
			else()
				target_link_libraries("${Tgt}" PRIVATE "${Link}")
			endif()
		else()
			target_link_libraries("${Tgt}" PRIVATE "${Link}")
		endif()
	endforeach()
endfunction()

# For a given subdirectory, set all configs to release.
function(make_release_only)
	if(CMAKE_CONFIGURATION_TYPES)
		set(ConfigTypes ${CMAKE_CONFIGURATION_TYPES})
		list(REMOVE_ITEM ConfigTypes "Release")
	else()
		set(ConfigTypes "Debug;MinSizeRel;RelWithDebInfo")
	endif()

	foreach(Config IN LISTS ConfigTypes)
		string(TOUPPER "${Config}" ConfigUpper)
		set("CMAKE_C_FLAGS_${ConfigUpper}" "${CMAKE_C_FLAGS_RELEASE}" PARENT_SCOPE)
	endforeach()
endfunction()

# As documented, OBJECT libraries in INTERFACE_LINK_LIBRARIES are treated as if
# they were INTERFACE libraries. So if we want to have an INTERFACE library
# which links to OBJECT libraries we need to copy those objects into the
# INTERFACE_SOURCES.
#
# This function should only be used on INTERFACE_LIBRARY targets.
function(propagate_object_links Tgt)
	get_property(Links TARGET "${Tgt}" PROPERTY INTERFACE_LINK_LIBRARIES)

	foreach(Link IN LISTS Links)
		if(TARGET "${Link}")
			get_property(LinkType TARGET "${Link}" PROPERTY TYPE)

			if(LinkType STREQUAL "OBJECT_LIBRARY")
				target_sources("${Tgt}" INTERFACE $<TARGET_OBJECTS:${Link}>)
			endif()
		endif()
	endforeach()
endfunction()

function(require_stricmp Tgt Visibility)
	check_function_exists(stricmp STRICMP_EXISTS)
	if(NOT STRICMP_EXISTS)
		target_compile_definitions(${Tgt} ${Visibility} stricmp=strcasecmp)
	endif()
endfunction()

function(require_strnicmp Tgt Visibility)
	check_function_exists(strnicmp STRNICMP_EXISTS)
	if(NOT STRNICMP_EXISTS)
		target_compile_definitions(${Tgt} ${Visibility} strnicmp=strncasecmp)
	endif()
endfunction()

function(use_fast_math)
	foreach(Tgt IN LISTS ARGN)
		if(TARGET Tgt)
			set(TgtType TARGET)
		else()
			set(TgtType SOURCE)
		endif()

		if(MSVC)
			set_property("${TgtType}" "${Tgt}" APPEND PROPERTY COMPILE_OPTIONS "/fp:fast")
		elseif(COMPILER_IS_GNUCXX_COMPATIBLE)
			set_property("${TgtType}" "${Tgt}" APPEND PROPERTY COMPILE_OPTIONS "-ffast-math" "-ffp-contract=fast")
		endif()
	endforeach()
endfunction()
