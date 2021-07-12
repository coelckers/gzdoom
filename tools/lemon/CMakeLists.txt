if(CMAKE_CROSSCOMPILING)
	return()
endif()

add_executable(lemon lemon.c)
set(CROSS_EXPORTS ${CROSS_EXPORTS} lemon PARENT_SCOPE)

# Lemon wants lempar.c in its directory
add_custom_command(TARGET lemon
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/lempar.c $<TARGET_FILE_DIR:lemon>
)
