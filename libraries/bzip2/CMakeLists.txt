cmake_minimum_required( VERSION 3.1...3.20 )

make_release_only()

add_library(bz2 STATIC
	blocksort.c
	bzlib.c
	compress.c
	crctable.c
	decompress.c
	huffman.c
	randtable.c
)

target_compile_definitions(bz2 PRIVATE BZ_NO_STDIO)

target_include_directories(bz2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
