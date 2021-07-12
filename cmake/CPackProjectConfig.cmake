# Adjusts global CPACK variables for specific generators.
# See CPACK_PROJECT_CONFIG_FILE.

if(CPACK_GENERATOR STREQUAL "ZIP")
	# We just want the files flat in the archive
	set(CPACK_PACKAGING_INSTALL_PREFIX "/")
	set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)

	# Conform to already existing package naming convention
	if(CPACK_SYSTEM_NAME STREQUAL "win64")
		set(PACKAGE_ARCH "64bit")
	else(CPACK_SYSTEM_NAME STREQUAL "win32")
		set(PACKAGE_ARCH "32bit")
	endif()
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Windows-${PACKAGE_ARCH}")
endif()
