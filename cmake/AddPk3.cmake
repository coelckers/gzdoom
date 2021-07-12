option(PK3_QUIET_ZIPDIR "Do not list files processed by zipdir" OFF)
if(PK3_QUIET_ZIPDIR)
	set(PK3_ZIPDIR_OPTIONS "-q")
endif()

# Recursive function to place PK3 archive source files into a hierarchy of source file in the IDE
function(assort_pk3_source_folder FOLDER_NAME PK3_DIR)
	# Assort source files into folders in the IDE
	file(GLOB PK3_SRCS ${PK3_DIR}/*) # Create list of all files in this folder
	foreach(PK3_SRC ${PK3_SRCS})
		# If there are subfolders, recurse into them
		if(IS_DIRECTORY ${PK3_SRC})
			get_filename_component(DIRNAME ${PK3_SRC} NAME)
			# Exclude folder from list of source files
			list(REMOVE_ITEM PK3_SRCS ${PK3_SRC})
			# Recurse deeper into the filesystem folder tree
			assort_pk3_source_folder(${FOLDER_NAME}\\${DIRNAME} ${PK3_SRC})
		endif()
		# Assign IDE group for current top-level source files
		source_group(${FOLDER_NAME} FILES ${PK3_SRCS})
	endforeach()
endfunction()

# Simplify pk3 building, add_pk3(filename srcdirectory)
function(add_pk3 PK3_NAME PK3_DIR)
	# Generate target name. Just use "pk3" for main pk3 target.
	string(REPLACE "." "_" PK3_TARGET ${PK3_NAME})

	if(NOT ZDOOM_OUTPUT_OLDSTYLE)
		add_custom_command(OUTPUT ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
			COMMAND zipdir -udf ${PK3_ZIPDIR_OPTIONS} ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} ${PK3_DIR}
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} $<TARGET_FILE_DIR:zdoom>/${PK3_NAME}
			DEPENDS zipdir
		)
	else()
		add_custom_command(OUTPUT ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
			COMMAND zipdir -udf ${PK3_ZIPDIR_OPTIONS} ${ZDOOM_OUTPUT_DIR}/${PK3_NAME} ${PK3_DIR}
			DEPENDS zipdir
		)
	endif()

	# Create a list of source files for this PK3, for use in the IDE
	# Phase 1: Create a list of all source files for this PK3 archive, except
	#  for a couple of strife image file names that confuse CMake.
	file(GLOB_RECURSE PK3_SRCS ${PK3_DIR}/*)
	# Exclude from the source list some files with brackets in the
	# file names here, because they confuse CMake.
	# This only affects the list of source files shown in the IDE.
	# It does not actually remove the files from the PK3 archive.
	# First replace that toxic bracket character with something we can handle
	string(REPLACE "[" confusing_bracket PK3_SRCS "${PK3_SRCS}")
	string(REPLACE "]" confusing_bracket PK3_SRCS "${PK3_SRCS}")
	foreach(PK3_SRC ${PK3_SRCS}) # All source files at all levels
		# Exclude those quarantined source file source file names that once had a bracket
		if(${PK3_SRC} MATCHES confusing_bracket)
			# message(STATUS "Ignoring PK3 file name containing brackets "${PK3_SRC})
			list(REMOVE_ITEM PK3_SRCS ${PK3_SRC})
		endif()
	endforeach()

	# Phase 2: Create the PK3 build rule, including the source file list for the IDE
	# Touch the zipdir executable here so that the pk3s are forced to
	# rebuild each time since their dependency has "changed."
	add_custom_target(${PK3_TARGET} ALL
		COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE:zipdir>
		DEPENDS ${ZDOOM_OUTPUT_DIR}/${PK3_NAME}
		SOURCES ${PK3_SRCS}
	)

	# Phase 3: Assign source files to a nice folder structure in the IDE
	assort_pk3_source_folder("Source Files" ${PK3_DIR})

	# Phase 4: Add the resulting PK3 to the install target.
	install(FILES "${PROJECT_BINARY_DIR}/${PK3_NAME}"
		DESTINATION ${CMAKE_INSTALL_DATADIR}
		COMPONENT "Game resources"
	)
endfunction()
