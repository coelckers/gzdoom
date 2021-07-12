cmake_minimum_required(VERSION 3.1...3.20)

#make_release_only()

project(asmjit C)

set(ASMJITNAME asmjit)

add_definitions(-DASMJIT_BUILD_EMBED)
add_definitions(-DASMJIT_STATIC)

if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

if(APPLE)
	# Suppress stdlib.h:334:6: warning: pointer is missing a nullability type specifier (_Nonnull, _Nullable, or _Null_unspecified)
	add_definitions(-Wno-nullability-completeness)
endif()

set(ASMJIT_PUBLIC_HDRS
    asmjit/arm.h
	asmjit/asmjit.h
    asmjit/asmjit_apibegin.h
    asmjit/asmjit_apiend.h
    asmjit/asmjit_build.h
    asmjit/base.h
    asmjit/base/arch.h
    asmjit/base/assembler.h
    asmjit/base/codebuilder.h
    asmjit/base/codecompiler.h
    asmjit/base/codeemitter.h
    asmjit/base/codeholder.h
    asmjit/base/constpool.h
    asmjit/base/cpuinfo.h
    asmjit/base/func.h
    asmjit/base/globals.h
    asmjit/base/inst.h
    asmjit/base/logging.h
    asmjit/base/misc_p.h
    asmjit/base/operand.h
    asmjit/base/osutils.h
    asmjit/base/regalloc_p.h
    asmjit/base/runtime.h
    asmjit/base/simdtypes.h
    asmjit/base/string.h
    asmjit/base/utils.h
    asmjit/base/vmem.h
    asmjit/base/zone.h
    asmjit/x86.h
    asmjit/x86/x86assembler.h
    asmjit/x86/x86builder.h
    asmjit/x86/x86compiler.h
    asmjit/x86/x86emitter.h
    asmjit/x86/x86globals.h
    asmjit/x86/x86inst.h
    asmjit/x86/x86instimpl_p.h
    asmjit/x86/x86internal_p.h
    asmjit/x86/x86logging_p.h
    asmjit/x86/x86misc.h
    asmjit/x86/x86operand.h
    asmjit/x86/x86regalloc_p.h
)
set(ASMJIT_SRCS
    asmjit/base/arch.cpp
    asmjit/base/assembler.cpp
    asmjit/base/codebuilder.cpp
    asmjit/base/codecompiler.cpp
    asmjit/base/codeemitter.cpp
    asmjit/base/codeholder.cpp
    asmjit/base/constpool.cpp
    asmjit/base/cpuinfo.cpp
    asmjit/base/func.cpp
    asmjit/base/globals.cpp
    asmjit/base/inst.cpp
    asmjit/base/logging.cpp
    asmjit/base/operand.cpp
    asmjit/base/osutils.cpp
    asmjit/base/regalloc.cpp
    asmjit/base/runtime.cpp
    asmjit/base/string.cpp
    asmjit/base/utils.cpp
    asmjit/base/vmem.cpp
    asmjit/base/zone.cpp
    asmjit/x86/x86assembler.cpp
    asmjit/x86/x86builder.cpp
    asmjit/x86/x86compiler.cpp
    asmjit/x86/x86inst.cpp
    asmjit/x86/x86instimpl.cpp
    asmjit/x86/x86internal.cpp
    asmjit/x86/x86logging.cpp
    asmjit/x86/x86operand.cpp
    asmjit/x86/x86operand_regs.cpp
    asmjit/x86/x86regalloc.cpp
)

add_library(${ASMJITNAME} STATIC ${ASMJIT_SRCS} ${ASMJIT_PUBLIC_HDRS})

set_target_properties(${ASMJITNAME} PROPERTIES OUTPUT_NAME asmjit)

target_include_directories(${ASMJITNAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL )
    install(TARGETS ${ASMJITNAME}
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib )
endif()
if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL )
    install(FILES ${ASMJIT_PUBLIC_HDRS} DESTINATION include)
endif()
