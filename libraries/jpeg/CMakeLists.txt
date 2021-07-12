cmake_minimum_required(VERSION 3.1...3.20)

make_release_only()

add_library(jpeg STATIC
	jaricom.c
	jcomapi.c
	jdapimin.c
	jdapistd.c
	jdarith.c
	jdatasrc.c
	jdcoefct.c
	jdcolor.c
	jddctmgr.c
	jdhuff.c
	jdinput.c
	jdmainct.c
	jdmarker.c
	jdmaster.c
	jdmerge.c
	jdpostct.c
	jdsample.c
	jerror.c
	jidctflt.c
	jidctfst.c
	jidctint.c
	jmemansi.c
	jmemmgr.c
	jquant1.c
	jquant2.c
	jutils.c
)

target_include_directories(jpeg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(COMPILER_IS_GNUC_COMPATIBLE)
	target_compile_options(jpeg PRIVATE "-Wall" "-Wextra" "-Wno-unused-parameter" "-fomit-frame-pointer")
endif()
