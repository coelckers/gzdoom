cmake_minimum_required( VERSION 3.1.0 )

make_release_only()

if(WIN32)
    add_subdirectory(OSDependent/Windows)
elseif(UNIX)
    add_subdirectory(OSDependent/Unix)
else(WIN32)
    message("unknown platform")
endif(WIN32)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    add_compile_options(-Wall -Wmaybe-uninitialized -Wuninitialized -Wunused -Wunused-local-typedefs
                        -Wunused-parameter -Wunused-value  -Wunused-variable -Wunused-but-set-parameter -Wunused-but-set-variable -fno-exceptions)
    add_compile_options(-Wno-reorder)  # disable this from -Wall, since it happens all over.
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    add_compile_options(-Wall -Wuninitialized -Wunused -Wunused-local-typedefs
                        -Wunused-parameter -Wunused-value  -Wunused-variable)
    add_compile_options(-Wno-reorder)  # disable this from -Wall, since it happens all over.
endif()

# Request C++11
if(${CMAKE_VERSION} VERSION_LESS 3.1)
    # CMake versions before 3.1 do not understand CMAKE_CXX_STANDARD
    # remove this block once CMake >=3.1 has fixated in the ecosystem
    add_compile_options(-std=c++11)
else()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(SOURCES
    MachineIndependent/glslang.y
    MachineIndependent/glslang_tab.cpp
    MachineIndependent/attribute.cpp
    MachineIndependent/Constant.cpp
    MachineIndependent/iomapper.cpp
    MachineIndependent/InfoSink.cpp
    MachineIndependent/Initialize.cpp
    MachineIndependent/IntermTraverse.cpp
    MachineIndependent/Intermediate.cpp
    MachineIndependent/ParseContextBase.cpp
    MachineIndependent/ParseHelper.cpp
    MachineIndependent/PoolAlloc.cpp
    MachineIndependent/RemoveTree.cpp
    MachineIndependent/Scan.cpp
    MachineIndependent/ShaderLang.cpp
    MachineIndependent/SymbolTable.cpp
    MachineIndependent/Versions.cpp
    MachineIndependent/intermOut.cpp
    MachineIndependent/limits.cpp
    MachineIndependent/linkValidate.cpp
    MachineIndependent/parseConst.cpp
    MachineIndependent/reflection.cpp
    MachineIndependent/preprocessor/Pp.cpp
    MachineIndependent/preprocessor/PpAtom.cpp
    MachineIndependent/preprocessor/PpContext.cpp
    MachineIndependent/preprocessor/PpScanner.cpp
    MachineIndependent/preprocessor/PpTokens.cpp
    MachineIndependent/propagateNoContraction.cpp
    GenericCodeGen/CodeGen.cpp
    GenericCodeGen/Link.cpp)

set(HEADERS
    Public/ShaderLang.h
    Include/arrays.h
    Include/BaseTypes.h
    Include/Common.h
    Include/ConstantUnion.h
    Include/InfoSink.h
    Include/InitializeGlobals.h
    Include/intermediate.h
    Include/PoolAlloc.h
    Include/ResourceLimits.h
    Include/revision.h
    Include/ShHandle.h
    Include/Types.h
    MachineIndependent/attribute.h
    MachineIndependent/glslang_tab.cpp.h
    MachineIndependent/gl_types.h
    MachineIndependent/Initialize.h
    MachineIndependent/iomapper.h
    MachineIndependent/LiveTraverser.h
    MachineIndependent/localintermediate.h
    MachineIndependent/ParseHelper.h
    MachineIndependent/reflection.h
    MachineIndependent/RemoveTree.h
    MachineIndependent/Scan.h
    MachineIndependent/ScanContext.h
    MachineIndependent/SymbolTable.h
    MachineIndependent/Versions.h
    MachineIndependent/parseVersions.h
    MachineIndependent/propagateNoContraction.h
    MachineIndependent/preprocessor/PpContext.h
    MachineIndependent/preprocessor/PpTokens.h)

# This might be useful for making grammar changes:
#
# find_package(BISON)
# add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/MachineIndependent/glslang_tab.cpp ${CMAKE_CURRENT_SOURCE_DIR}/MachineIndependent/glslang_tab.cpp.h
#                    COMMAND ${BISON_EXECUTABLE} --defines=${CMAKE_CURRENT_SOURCE_DIR}/MachineIndependent/glslang_tab.cpp.h -t ${CMAKE_CURRENT_SOURCE_DIR}/MachineIndependent/glslang.y -o ${CMAKE_CURRENT_SOURCE_DIR}/MachineIndependent/glslang_tab.cpp
#                    MAIN_DEPENDENCY MachineIndependent/glslang.y
#                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# set(BISON_GLSLParser_OUTPUT_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/MachineIndependent/glslang_tab.cpp)

# Precompiled header macro. Parameters are source file list and filename for pch cpp file.
macro(glslang_pch SRCS PCHCPP)
  if(MSVC AND CMAKE_GENERATOR MATCHES "^Visual Studio")
    set(PCH_NAME "$(IntDir)\\pch.pch")
    # make source files use/depend on PCH_NAME
    set_source_files_properties(${${SRCS}} PROPERTIES COMPILE_FLAGS "/Yupch.h /FIpch.h /Fp${PCH_NAME} /Zm300" OBJECT_DEPENDS "${PCH_NAME}")
    # make PCHCPP file compile and generate PCH_NAME
    set_source_files_properties(${PCHCPP} PROPERTIES COMPILE_FLAGS "/Ycpch.h /Fp${PCH_NAME} /Zm300" OBJECT_OUTPUTS "${PCH_NAME}")
    list(APPEND ${SRCS} "${PCHCPP}")
  endif()
endmacro(glslang_pch)

glslang_pch(SOURCES MachineIndependent/pch.cpp)

add_library(glslang STATIC ${LIB_TYPE} ${BISON_GLSLParser_OUTPUT_SOURCE} ${SOURCES} ${HEADERS})
set_property(TARGET glslang PROPERTY FOLDER glslang)
set_property(TARGET glslang PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(glslang OGLCompiler OSDependent)
target_include_directories(glslang PUBLIC Public)

use_fast_math(glslang)

if(WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(glslang PROPERTIES PREFIX "")
endif()

if(ENABLE_HLSL)
    target_link_libraries(glslang HLSL)
endif()

if(WIN32)
    source_group("Public" REGULAR_EXPRESSION "Public/*")
    source_group("MachineIndependent" REGULAR_EXPRESSION "MachineIndependent/[^/]*")
    source_group("Include" REGULAR_EXPRESSION "Include/[^/]*")
    source_group("GenericCodeGen" REGULAR_EXPRESSION "GenericCodeGen/*")
    source_group("MachineIndependent\\Preprocessor" REGULAR_EXPRESSION "MachineIndependent/preprocessor/*")
endif(WIN32)

if(ENABLE_GLSLANG_INSTALL)
    if(BUILD_SHARED_LIBS)
        install(TARGETS glslang
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    else()
        install(TARGETS glslang
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endif(ENABLE_GLSLANG_INSTALL)

if(ENABLE_GLSLANG_INSTALL)
    foreach(file ${HEADERS})
        get_filename_component(dir ${file} DIRECTORY)
        install(FILES ${file} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/glslang/${dir})
    endforeach()
endif(ENABLE_GLSLANG_INSTALL)
