set(INKSCAPE_LIBS "")
set(INKSCAPE_CXX_FLAGS "")
set(INKSCAPE_CXX_FLAGS_DEBUG "")

include_directories(
    ${PROJECT_SOURCE_DIR}/src

    # generated includes
    ${CMAKE_BINARY_DIR}/include
)

# NDEBUG implies G_DISABLE_ASSERT
string(TOUPPER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_UPPER)
if(CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE_UPPER} MATCHES "-DNDEBUG")
    list(APPEND INKSCAPE_CXX_FLAGS "-DG_DISABLE_ASSERT")
endif()

# AddressSanitizer
# Clang's AddressSanitizer can detect more memory errors and is more powerful
# than compiling with _FORTIFY_SOURCE but has a performance impact (approx. 2x
# slower), so it's not suitable for release builds.
if(WITH_ASAN)
    list(APPEND INKSCAPE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
    list(APPEND INKSCAPE_LIBS "-fsanitize=address")
else()
    # Undefine first, to suppress 'warning: "_FORTIFY_SOURCE" redefined'
    list(APPEND INKSCAPE_CXX_FLAGS "-U_FORTIFY_SOURCE")
    list(APPEND INKSCAPE_CXX_FLAGS "-D_FORTIFY_SOURCE=2")
endif()

# Errors for common mistakes
list(APPEND INKSCAPE_CXX_FLAGS "-fstack-protector-strong")
list(APPEND INKSCAPE_CXX_FLAGS "-Werror=format")                # e.g.: printf("%s", std::string("foo"))
list(APPEND INKSCAPE_CXX_FLAGS "-Werror=format-security")       # e.g.: printf(variable);
list(APPEND INKSCAPE_CXX_FLAGS "-Werror=ignored-qualifiers")    # e.g.: const int foo();
list(APPEND INKSCAPE_CXX_FLAGS "-Werror=return-type")           # non-void functions that don't return a value
list(APPEND INKSCAPE_CXX_FLAGS "-Werror=vla")                   # variable-length arrays
list(APPEND INKSCAPE_CXX_FLAGS "-Wno-switch")                   # See !849 for discussion
list(APPEND INKSCAPE_CXX_FLAGS "-Wmisleading-indentation")
list(APPEND INKSCAPE_CXX_FLAGS_DEBUG "-Wcomment")
list(APPEND INKSCAPE_CXX_FLAGS_DEBUG "-Wunused-function")
list(APPEND INKSCAPE_CXX_FLAGS_DEBUG "-Wunused-variable")
list(APPEND INKSCAPE_CXX_FLAGS_DEBUG "-D_GLIBCXX_ASSERTIONS")
if (CMAKE_COMPILER_IS_GNUCC)
    list(APPEND INKSCAPE_CXX_FLAGS "-Wstrict-null-sentinel")    # For NULL instead of nullptr
    list(APPEND INKSCAPE_CXX_FLAGS_DEBUG "-fexceptions -grecord-gcc-switches -fasynchronous-unwind-tables")
    if(CXX_COMPILER_VERSION VERSION_GREATER 8.0)
        list(APPEND INKSCAPE_CXX_FLAGS_DEBUG "-fstack-clash-protection -fcf-protection")
    endif()
endif()

# Define the flags for profiling if desired:
if(WITH_PROFILING)
    set(BUILD_SHARED_LIBS off)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif()

include(CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES("
#include <atomic>
#include <cstdint>
std::atomic<uint64_t> x (0);
int main() {
  uint64_t i = x.load(std::memory_order_relaxed);
  return 0;
}
"
LIBATOMIC_NOT_NEEDED)
IF (NOT LIBATOMIC_NOT_NEEDED)
    message(STATUS "  Adding -latomic to the libs.")
    list(APPEND INKSCAPE_LIBS "-latomic")
ENDIF()


# ----------------------------------------------------------------------------
# Files we include
# ----------------------------------------------------------------------------
if(WIN32)
    # Set the link and include directories
    get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

    list(APPEND INKSCAPE_LIBS "-lmscms")
    list(APPEND INKSCAPE_LIBS "-ldwmapi")

    list(APPEND INKSCAPE_CXX_FLAGS "-mms-bitfields")
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
      list(APPEND INKSCAPE_CXX_FLAGS "-mwindows")
      list(APPEND INKSCAPE_CXX_FLAGS "-mthreads")
    endif()

    list(APPEND INKSCAPE_LIBS "-lwinpthread")

    if(HAVE_MINGW64)
        list(APPEND INKSCAPE_CXX_FLAGS "-m64")
    else()
        list(APPEND INKSCAPE_CXX_FLAGS "-m32")
    endif()

    # Fixes for windows.h and GTK4
    add_definitions(-DNOGDI)
    add_definitions(-D_NO_W32_PSEUDO_MODIFIERS)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(INKSCAPE_DEP REQUIRED IMPORTED_TARGET
                  harfbuzz>=2.6.5
                  pangocairo>=1.44
                  pangoft2
                  fontconfig
                  gmodule-2.0
                  bdw-gc #boehm-demers-weiser gc
                  lcms2)

list(APPEND INKSCAPE_LIBS PkgConfig::INKSCAPE_DEP)

if(WITH_JEMALLOC)
    find_package(JeMalloc)
    if (JEMALLOC_FOUND)
        list(APPEND INKSCAPE_LIBS ${JEMALLOC_LIBRARIES})
    else()
        set(WITH_JEMALLOC OFF)
    endif()
endif()

pkg_search_module(ICU_UC REQUIRED IMPORTED_TARGET icu-uc)
list(APPEND INKSCAPE_LIBS PkgConfig::ICU_UC)

find_package(Iconv REQUIRED)
list(APPEND INKSCAPE_LIBS Iconv::Iconv)

find_package(Intl REQUIRED)
list(APPEND INKSCAPE_LIBS Intl::Intl)

find_package(GSL REQUIRED)
list(APPEND INKSCAPE_LIBS GSL::gsl)

find_package(double-conversion CONFIG REQUIRED)
list(APPEND INKSCAPE_LIBS double-conversion::double-conversion)

# Qt6 is the toolkit for Linea. Hard requirement.
find_package(Qt6 6.2 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Xml REQUIRED)
qt_standard_project_setup()

# Locate Qt deployment tools (macdeployqt on macOS, windeployqt on Windows)
if(APPLE)
    # Prefer the umbrella "qt" install's macdeployqt: Homebrew splits Qt 6 into
    # per-module packages and qtbase's macdeployqt only sees qtbase's own
    # frameworks (qtsvg, qtpdf, ... live in other prefixes).
    find_program(BREW_EXECUTABLE brew)
    if(BREW_EXECUTABLE)
        execute_process(
            COMMAND "${BREW_EXECUTABLE}" --prefix qt
            OUTPUT_VARIABLE QT_BREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    find_program(QT_DEPLOY_TOOL macdeployqt
        HINTS "${QT_BREW_PREFIX}/bin" ${QT6_BIN_DIR}
        REQUIRED)
endif()

# Check for system-wide version of 2geom and fallback to internal copy if not found
if(NOT WITH_INTERNAL_2GEOM)
    find_package(2Geom ${INKSCAPE_VERSION_MAJOR}.${INKSCAPE_VERSION_MINOR} CONFIG QUIET)
    if(NOT TARGET 2Geom::2geom)
        set(WITH_INTERNAL_2GEOM ON CACHE BOOL "Prefer internal copy of lib2geom" FORCE)
        message(STATUS "lib2geom not found, using internal copy in src/3rdparty/2geom")
    endif()
endif()

if(NOT WITH_INTERNAL_UEMF)
    find_package(Uemf 1.0.0 CONFIG QUIET)
    if(NOT TARGET Uemf::uemf)
        set(WITH_INTERNAL_UEMF ON CACHE BOOL "Prefer internal copy of libuemf" FORCE)
    endif()
endif()

if(NOT WITH_INTERNAL_DEPIXELIZE)
    find_package(Depixelize 0.1.0 CONFIG QUIET)
    if(NOT TARGET Depixelize::depixelize)
        set(WITH_INTERNAL_DEPIXELIZE ON CACHE BOOL "Prefer internal copy of libdepixelize" FORCE)
    endif()
endif()

if(NOT WITH_INTERNAL_AUTOTRACE)
    pkg_check_modules(AUTOTRACE QUIET IMPORTED_TARGET autotrace>=0.31.10)
    if(AUTOTRACE_FOUND)
        add_library(autotrace_LIB ALIAS PkgConfig::AUTOTRACE)
    else()
        set(WITH_INTERNAL_AUTOTRACE ON CACHE BOOL "Prefer internal copy of autotrace" FORCE)
    endif()
endif()

if(NOT WITH_INTERNAL_ADAPTAGRAMS)
    pkg_check_modules(ADAPTAGRAMS QUIET IMPORTED_TARGET libavoid>=0.1 libcola>=0.1 libvpsc>=0.1)
    if(ADAPTAGRAMS_FOUND)
        add_library(adaptagrams_LIB ALIAS PkgConfig::ADAPTAGRAMS)
    else()
        set(WITH_INTERNAL_ADAPTAGRAMS ON CACHE BOOL "Prefer internal copy of adaptagrams" FORCE)
    endif()
endif()

if(WITH_CAPYPDF)
  pkg_check_modules(CAPYPDF IMPORTED_TARGET capypdf>=0.18)
  if(CAPYPDF_FOUND)
    add_library(Inkscape::CapyPDF ALIAS PkgConfig::CAPYPDF)
  else()
    if(APPLE)
      message(STATUS "New CMYK PDF exporter disabled on macOS")
      set(WITH_CAPYPDF OFF)
    else()
      set(CAPY_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/deps)
      set(CAPY_LIBDIR ${CAPY_PREFIX}/${CMAKE_INSTALL_LIBDIR})
      include(ExternalProject)
      ExternalProject_Add(capypdf
          URL https://github.com/jpakkane/capypdf/releases/download/0.18.0/capypdf-0.18.0.tar.xz
          URL_HASH SHA256=bda2c0cbbc60b461b1c5d64c50cdecfee6e90b3c9ee28d33212412771caaafd4
          DOWNLOAD_EXTRACT_TIMESTAMP TRUE
          CONFIGURE_COMMAND meson setup . ../capypdf --libdir=${CAPY_LIBDIR} --prefix=${CAPY_PREFIX}
          BUILD_COMMAND meson compile
          INSTALL_COMMAND meson install
      )
      add_library(CapyPDF_LIB INTERFACE)
      target_include_directories(CapyPDF_LIB INTERFACE "${CAPY_PREFIX}/include/capypdf-0")
      target_link_directories(CapyPDF_LIB INTERFACE "${CAPY_LIBDIR}")
      target_link_libraries(CapyPDF_LIB INTERFACE -lcapypdf)
      add_library(Inkscape::CapyPDF ALIAS CapyPDF_LIB)
      list(APPEND CMAKE_INSTALL_RPATH ${CAPY_LIBDIR})
      if(WIN32)
        # DLL needs to be copied for tests to run
        ExternalProject_Add_Step(capypdf copy-dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CAPY_PREFIX}/bin/libcapypdf-0.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcapypdf-0.dll"
            DEPENDEES install
            BYPRODUCTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libcapypdf-0.dll"
        )
      endif()
    endif()
  endif()
endif()
if(WITH_CAPYPDF)
  list(APPEND INKSCAPE_LIBS Inkscape::CapyPDF)
  add_definitions(-DWITH_CAPYPDF)
endif()

if(WITH_POPPLER)
    pkg_check_modules(POPPLER IMPORTED_TARGET poppler>=0.20.0 poppler-glib>=0.20.0)
    if(POPPLER_FOUND)
        list(APPEND INKSCAPE_LIBS PkgConfig::POPPLER)
        set(HAVE_POPPLER_CAIRO ${ENABLE_POPPLER_CAIRO})
        add_definitions(-DWITH_POPPLER)
    else()
        set(WITH_POPPLER OFF)
        set(ENABLE_POPPLER_CAIRO OFF)
    endif()
else()
    set(ENABLE_POPPLER_CAIRO OFF)
endif()

if(WITH_LIBWPG)
    pkg_check_modules(LIBWPG IMPORTED_TARGET libwpg-0.3 librevenge-0.0 librevenge-stream-0.0)
    if(LIBWPG_FOUND)
        list(APPEND INKSCAPE_LIBS PkgConfig::LIBWPG)
    else()
        set(WITH_LIBWPG OFF)
    endif()
endif()

if(WITH_LIBVISIO)
    pkg_check_modules(LIBVISIO IMPORTED_TARGET libvisio-0.1 librevenge-0.0 librevenge-stream-0.0)
    if(LIBVISIO_FOUND)
        list(APPEND INKSCAPE_LIBS PkgConfig::LIBVISIO)
    else()
        set(WITH_LIBVISIO OFF)
    endif()
endif()

if(WITH_LIBCDR)
    pkg_check_modules(LIBCDR IMPORTED_TARGET libcdr-0.1 librevenge-0.0 librevenge-stream-0.0)
    if(LIBCDR_FOUND)
        list(APPEND INKSCAPE_LIBS PkgConfig::LIBCDR)
    else()
        set(WITH_LIBCDR OFF)
    endif()
endif()

find_package(JPEG)
if(JPEG_FOUND)
    list(APPEND INKSCAPE_LIBS JPEG::JPEG)
    set(HAVE_JPEG ON)
endif()

find_package(PNG REQUIRED)
list(APPEND INKSCAPE_LIBS PNG::PNG)

find_package(Potrace REQUIRED)
list(APPEND INKSCAPE_LIBS Potrace::Potrace)

if(WITH_SVG2)
    add_definitions(-DWITH_MESH -DWITH_CSSBLEND -DWITH_SVG2)
else()
    add_definitions(-UWITH_MESH -UWITH_CSSBLEND -UWITH_SVG2)
endif()

# ----------------------------------------------------------------------------
# CMake's builtin
# ----------------------------------------------------------------------------

# Include dependencies:

pkg_check_modules(
    MM REQUIRED IMPORTED_TARGET
    cairomm-1.16
    pangomm-2.48
)
list(APPEND INKSCAPE_LIBS PkgConfig::MM)

pkg_check_modules(GLIBMM IMPORTED_TARGET glibmm-2.68>=2.78.1 giomm-2.68)
if(GLIBMM_FOUND)
    add_library(GLibmm::GLibmm ALIAS PkgConfig::GLIBMM)
else()
    message("GLIBMM too old, glibmm 2.78.1 will be compiled from source")
    include(ExternalProject)
    ExternalProject_Add(glibmm
        URL https://download.gnome.org/sources/glibmm/2.78/glibmm-2.78.1.tar.xz
        URL_HASH SHA256=f473f2975d26c3409e112ed11ed36406fb3843fa975df575c22d4cb843085f61
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CONFIGURE_COMMAND meson setup --libdir lib . ../glibmm --prefix=${CMAKE_CURRENT_BINARY_DIR}/deps
        BUILD_COMMAND meson compile
        INSTALL_COMMAND meson install
    )
    add_library(GLibmm_LIB INTERFACE)
    target_include_directories(GLibmm_LIB INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/deps/include/glibmm-2.68/ ${CMAKE_CURRENT_BINARY_DIR}/deps/lib/glibmm-2.68/include ${CMAKE_CURRENT_BINARY_DIR}/deps/include/giomm-2.68/ ${CMAKE_CURRENT_BINARY_DIR}/deps/lib/giomm-2.68/include)
    target_link_directories(GLibmm_LIB INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/deps/lib)
    target_link_libraries(GLibmm_LIB INTERFACE -lglibmm-2.68)
    add_library(GLibmm::GLibmm ALIAS GLibmm_LIB)
endif()
list(APPEND INKSCAPE_LIBS GLibmm::GLibmm)

set(WITH_LIBSPELLING OFF)
set(WITH_GSOURCEVIEW OFF)

# stacktrace print on crash
if(WIN32)
    find_package(Boost 1.19.0 REQUIRED COMPONENTS stacktrace_windbg)
    list(APPEND INKSCAPE_LIBS "-lole32")
    list(APPEND INKSCAPE_LIBS "-ldbgeng")
    add_definitions("-DBOOST_STACKTRACE_USE_WINDBG")
elseif(APPLE)
    find_package(Boost 1.19.0 REQUIRED COMPONENTS stacktrace_basic)
    list(APPEND INKSCAPE_CXX_FLAGS "-D_GNU_SOURCE")
else()
    find_package(Boost 1.19.0 REQUIRED)
    # The package stacktrace_backtrace may not be available on all distros.
    find_package(Boost 1.19.0 COMPONENTS stacktrace_backtrace)
    if (BOOST_FOUND)
        list(APPEND INKSCAPE_LIBS "-lbacktrace")
        add_definitions("-DBOOST_STACKTRACE_USE_BACKTRACE")
    else() # fall back to stacktrace_basic
        find_package(Boost 1.19.0 REQUIRED COMPONENTS stacktrace_basic)
        list(APPEND INKSCAPE_CXX_FLAGS "-D_GNU_SOURCE")
    endif()
endif()



if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 7 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    list(APPEND INKSCAPE_LIBS "-lstdc++fs")
endif()

list(APPEND INKSCAPE_LIBS Boost::headers)

find_package(LibXslt REQUIRED)
list(APPEND INKSCAPE_LIBS LibXslt::LibXslt)

find_package(LibXml2 REQUIRED)
list(APPEND INKSCAPE_LIBS LibXml2::LibXml2)

find_package(ZLIB REQUIRED)
list(APPEND INKSCAPE_LIBS ZLIB::ZLIB)

if(WITH_GNU_READLINE)
  pkg_check_modules(Readline IMPORTED_TARGET readline)
  if(Readline_FOUND)
    message(STATUS "Found GNU Readline: ${Readline_LIBRARY}")
    list(APPEND INKSCAPE_LIBS PkgConfig::Readline)
  else()
    message(STATUS "Did not find GNU Readline")
    set(WITH_GNU_READLINE OFF)
  endif()
endif()

if(WITH_IMAGE_MAGICK)
    # we want "<" but pkg_check_modules only offers "<=" for some reason; let's hope nobody actually has 7.0.0
    pkg_check_modules(MAGICK IMPORTED_TARGET ImageMagick++<=7)
    if(MAGICK_FOUND)
        set(WITH_GRAPHICS_MAGICK OFF)  # prefer ImageMagick for now and disable GraphicsMagick if found
    else()
        set(WITH_IMAGE_MAGICK OFF)
    endif()
endif()
if(WITH_GRAPHICS_MAGICK)
    pkg_check_modules(MAGICK IMPORTED_TARGET GraphicsMagick++)
    if(NOT MAGICK_FOUND)
        set(WITH_GRAPHICS_MAGICK OFF)
    endif()
endif()
if(MAGICK_FOUND)
    list(APPEND INKSCAPE_LIBS PkgConfig::MAGICK)
    set(WITH_MAGICK ON) # enable 'Extensions > Raster'
endif()

set(ENABLE_NLS OFF)
if(WITH_NLS)
    find_package(Gettext)
    if(GETTEXT_FOUND)
        message(STATUS "Found gettext + msgfmt to convert language files. Translation enabled")
        set(ENABLE_NLS ON)
    else(GETTEXT_FOUND)
        message(STATUS "Cannot find gettext + msgfmt to convert language file. Translation won't be enabled")
        set(WITH_NLS OFF)
    endif(GETTEXT_FOUND)
    find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
    if(GETTEXT_XGETTEXT_EXECUTABLE)
        message(STATUS "Found xgettext. inkscape.pot will be re-created if missing.")
    else()
        message(STATUS "Did not find xgettext. inkscape.pot can't be re-created.")
    endif()
endif(WITH_NLS)

pkg_check_modules(SIGC++ REQUIRED IMPORTED_TARGET sigc++-3.0>=3.6)
list(APPEND INKSCAPE_LIBS PkgConfig::SIGC++)
list(APPEND INKSCAPE_CXX_FLAGS "-DSIGCXX_DISABLE_DEPRECATED")

pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)
list(APPEND INKSCAPE_LIBS PkgConfig::EPOXY)


# end Dependencies



# Set include directories and CXX flags
# (INKSCAPE_LIBS are set as target_link_libraries for linea_base in src/CMakeLists.txt)

foreach(flag ${INKSCAPE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endforeach()
foreach(flag ${INKSCAPE_CXX_FLAGS_DEBUG})
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${flag}")
endforeach()

# Add color output to ninja
if ("${CMAKE_GENERATOR}" MATCHES "Ninja")
    add_compile_options (-fdiagnostics-color)
endif ()

list(REMOVE_DUPLICATES INKSCAPE_LIBS)

include(${CMAKE_CURRENT_LIST_DIR}/ConfigChecks.cmake) # TODO: Check if this needs to be "hidden" here

unset(INKSCAPE_CXX_FLAGS)
unset(INKSCAPE_CXX_FLAGS_DEBUG)
