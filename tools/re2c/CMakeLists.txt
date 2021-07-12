if(CMAKE_CROSSCOMPILING)
	return()
endif()

include( CheckFunctionExists )
include( CheckTypeSize )

set(PACKAGE_NAME re2c)
set(PACKAGE_TARNAME re2c)
set(PACKAGE_VERSION 0.16)
set(PACKAGE_STRING "re2c 0.16")
set(PACKAGE_BUGREPORT "re2c-general@lists.sourceforge.net")

check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strndup HAVE_STRNDUP)

check_type_size("0i8" SIZEOF_0I8)
check_type_size("0l" SIZEOF_0L)
check_type_size("0ll" SIZEOF_0LL)
check_type_size(char SIZEOF_CHAR)
check_type_size(short SIZEOF_SHORT)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)
check_type_size("void *" SIZEOF_VOID_P)
check_type_size(__int64 SIZEOF___INT_64)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_executable(re2c
	src/codegen/bitmap.cc
	src/codegen/emit_action.cc
	src/codegen/emit_dfa.cc
	src/codegen/label.cc
	src/codegen/go_construct.cc
	src/codegen/go_destruct.cc
	src/codegen/go_emit.cc
	src/codegen/go_used_labels.cc
	src/codegen/input_api.cc
	src/codegen/output.cc
	src/codegen/print.cc
	src/conf/msg.cc
	src/conf/opt.cc
	src/conf/parse_opts.cc
	src/conf/warn.cc
	src/ir/nfa/calc_size.cc
	src/ir/nfa/nfa.cc
	src/ir/nfa/split.cc
	src/ir/adfa/adfa.cc
	src/ir/adfa/prepare.cc
	src/ir/dfa/determinization.cc
	src/ir/dfa/fillpoints.cc
	src/ir/dfa/minimization.cc
	src/ir/regexp/display.cc
	src/ir/regexp/encoding/enc.cc
	src/ir/regexp/encoding/range_suffix.cc
	src/ir/regexp/encoding/utf8/utf8_regexp.cc
	src/ir/regexp/encoding/utf8/utf8_range.cc
	src/ir/regexp/encoding/utf8/utf8.cc
	src/ir/regexp/encoding/utf16/utf16_regexp.cc
	src/ir/regexp/encoding/utf16/utf16.cc
	src/ir/regexp/encoding/utf16/utf16_range.cc
	src/ir/regexp/fixed_length.cc
	src/ir/regexp/regexp.cc
	src/ir/compile.cc
	src/ir/rule_rank.cc
	src/ir/skeleton/control_flow.cc
	src/ir/skeleton/generate_code.cc
	src/ir/skeleton/generate_data.cc
	src/ir/skeleton/match_empty.cc
	src/ir/skeleton/maxlen.cc
	src/ir/skeleton/skeleton.cc
	src/ir/skeleton/unreachable.cc
	src/ir/skeleton/way.cc
	src/main.cc
	src/parse/code.cc
	src/parse/input.cc
	src/parse/lex.cc
	src/parse/lex_conf.cc
	src/parse/parser.cc
	src/parse/scanner.cc
	src/parse/unescape.cc
	src/util/s_to_n32_unsafe.cc
	src/util/range.cc
)

file(GLOB SRC_HDR
	src/codegen/*.h
	src/conf/*.h
	src/ir/*.h
	src/*.h
	src/parse/*.h
	src/util/*.h
)
target_sources(re2c PRIVATE ${SRC_HDR})

target_include_directories(re2c PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(re2c PRIVATE HAVE_CONFIG_H)

if(MSVC)
	# Runtime type information is required and don't complain about uint32_t to bool conversions
	target_compile_options(re2c PRIVATE /GR /wd4800)
endif()

set(CROSS_EXPORTS ${CROSS_EXPORTS} re2c PARENT_SCOPE)
