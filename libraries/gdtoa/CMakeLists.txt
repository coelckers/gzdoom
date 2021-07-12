cmake_minimum_required( VERSION 3.1...3.20 )

add_library(gdtoa STATIC
	dmisc.c
	dtoa.c
	misc.c
)

target_include_directories(gdtoa PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(gdtoa PRIVATE INFNAN_CHECK MULTIPLE_THREADS)

# Disable warnings for << operator precedence (4554) and
# unreferenced labels (4102) from VC
if(MSVC)
	target_compile_options(gdtoa PRIVATE "/wd4554" "/wd4102")
endif()

if(COMPILER_IS_GNUC_COMPATIBLE)
	target_compile_options(gdtoa PRIVATE "-Wall" "-Wextra")
endif()

if(NOT MSVC AND NOT APPLE)
	if(CMAKE_CROSSCOMPILING)
		message(WARNING "arithchk and qnan are going to be run on the host system. This will probably produce incorrect results when cross compiling.")
	endif()

	if(NOT CMAKE_CROSSCOMPILING)
		add_executable(arithchk arithchk.c)
		set(CROSS_EXPORTS ${CROSS_EXPORTS} arithchk PARENT_SCOPE)
	endif()
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/arith.h 
		COMMAND arithchk >${CMAKE_CURRENT_BINARY_DIR}/arith.h
		DEPENDS arithchk
	)

	if(NOT CMAKE_CROSSCOMPILING)
		add_executable(qnan qnan.c arith.h)
		target_include_directories(qnan PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
		set( CROSS_EXPORTS ${CROSS_EXPORTS} qnan PARENT_SCOPE )
	endif()
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gd_qnan.h
		COMMAND qnan >${CMAKE_CURRENT_BINARY_DIR}/gd_qnan.h
		DEPENDS qnan
	)

	target_sources(gdtoa PRIVATE arith.h gd_qnan.h)

	target_include_directories(gdtoa PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endif()
