# SPDX-License-Identifier: GPL-2.0-or-later
# Embedded Lua 5.5.1 and Linea's script adapter.

set(LINEA_LUA_CORE
    lua/src/lapi.c
    lua/src/lcode.c
    lua/src/lctype.c
    lua/src/ldebug.c
    lua/src/ldo.c
    lua/src/ldump.c
    lua/src/lfunc.c
    lua/src/lgc.c
    lua/src/llex.c
    lua/src/lmem.c
    lua/src/lobject.c
    lua/src/lopcodes.c
    lua/src/lparser.c
    lua/src/lstate.c
    lua/src/lstring.c
    lua/src/ltable.c
    lua/src/ltm.c
    lua/src/lundump.c
    lua/src/lvm.c
    lua/src/lzio.c
)

set(LINEA_LUA_LIBS
    lua/src/lauxlib.c
    lua/src/lbaselib.c
    lua/src/lcorolib.c
    lua/src/ldblib.c
    lua/src/liolib.c
    lua/src/lmathlib.c
    lua/src/loadlib.c
    lua/src/loslib.c
    lua/src/lstrlib.c
    lua/src/ltablib.c
    lua/src/lutf8lib.c
    lua/src/linit.c
)

target_sources(linea_base PRIVATE
    ${LINEA_LUA_CORE}
    ${LINEA_LUA_LIBS}
    script-error.h
    script-error.cpp
    script-engine.h
    script-engine.cpp
    script-bindings.h
    script-bindings.cpp
    script-registry.h
    script-registry.cpp
    test-script.h
)

target_include_directories(linea_base PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lua/src
)

if(APPLE)
    target_compile_definitions(linea_base PRIVATE LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(linea_base PRIVATE LUA_USE_LINUX)
elseif(WIN32)
    target_compile_definitions(linea_base PRIVATE LUA_USE_WINDOWS)
endif()

set_source_files_properties(${LINEA_LUA_CORE} ${LINEA_LUA_LIBS}
    PROPERTIES
    LANGUAGE C
)
