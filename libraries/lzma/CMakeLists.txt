cmake_minimum_required(VERSION 3.1...3.20)

make_release_only()

add_library(lzma STATIC
	C/7zArcIn.c
	C/7zBuf.c
	C/7zCrc.c
	C/7zCrcOpt.c
	C/7zDec.c
	C/7zStream.c
	C/Bcj2.c
	C/Bra.c
	C/Bra86.c
	C/BraIA64.c
	C/CpuArch.c
	C/Delta.c
	C/LzFind.c
	C/Lzma2Dec.c
	C/LzmaDec.c
	C/LzmaEnc.c
	C/Ppmd7.c
	C/Ppmd7Dec.c
)

if(WIN32)
	target_sources(lzma PRIVATE C/LzFindMt.c C/Threads.c)
else()
	target_compile_definitions(lzma PRIVATE _7ZIP_ST)
endif()

target_compile_definitions(lzma PRIVATE _7ZIP_PPMD_SUPPPORT)

target_include_directories(lzma PUBLIC C)
