# EXCLUDE_FROM_ALL: the bundled libraries' own install rules (headers, cmake
# and pkg-config files) are not needed - the app only needs their binaries,
# which are bundled via install(TARGETS) in src/CMakeLists.txt.
if(WITH_INTERNAL_2GEOM)
    set(2GEOM_BUILD_SHARED ${BUILD_SHARED_LIBS} CACHE BOOL "")
    set(2GEOM_INSTALL OFF CACHE BOOL "" FORCE)
    include(FetchContent)
    FetchContent_Declare(2Geom SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/2geom" OVERRIDE_FIND_PACKAGE EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(2Geom)
endif()

if(WITH_INTERNAL_UEMF)
    add_subdirectory(libuemf EXCLUDE_FROM_ALL)
endif()

add_subdirectory(libcroco EXCLUDE_FROM_ALL)

if(WITH_INTERNAL_DEPIXELIZE)
    add_subdirectory(libdepixelize EXCLUDE_FROM_ALL)
endif()

if(WITH_INTERNAL_ADAPTAGRAMS)
    add_subdirectory(adaptagrams EXCLUDE_FROM_ALL)
endif()

if(WITH_INTERNAL_AUTOTRACE)
    add_subdirectory(autotrace EXCLUDE_FROM_ALL)
endif()
