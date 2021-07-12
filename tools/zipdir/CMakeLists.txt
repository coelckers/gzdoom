if(CMAKE_CROSSCOMPILING)
	return()
endif()

add_executable(zipdir zipdir.c)
target_link_libraries(zipdir PRIVATE ZLIB::ZLIB BZip2::BZip2 lzma)

if(TARGET musl-fts)
	target_link_libraries(zipdir PRIVATE musl-fts)
endif()

set(CROSS_EXPORTS ${CROSS_EXPORTS} zipdir PARENT_SCOPE)
