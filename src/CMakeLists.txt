# SPDX-License-Identifier: GPL-2.0-or-later

add_library(linea_base)

target_sources(linea_base PRIVATE
  alignment-snapper.cpp
  attribute-rel-css.cpp
  attribute-rel-svg.cpp
  attribute-rel-util.cpp
  attribute-sort-util.cpp
  attributes.cpp
  auto-save.cpp
  axis-manip.cpp
  composite-undo-stack-observer.cpp
  conditions.cpp
  conn-avoid-ref.cpp
  context-fns.cpp
  css-chemistry.cpp
  desktop-events.cpp
  desktop-style.cpp
  desktop.cpp
  distribution-snapper.cpp
  document-subset.cpp
  document-undo.cpp
  document-update.cpp
  document.cpp
  event-log.cpp
  file.cpp
  filter-chemistry.cpp
  filter-enums.cpp
  gc-anchored.cpp
  gradient-chemistry.cpp
  gradient-drag.cpp
  guide-snapper.cpp
  grid-snapper.cpp
  id-clash.cpp
  inkscape.cpp
  inkscape-version-info.cpp
  layer-manager.cpp
  line-geometry.cpp
  line-snapper.cpp
  media.cpp
  message-context.cpp
  message-stack.cpp
  mod360.cpp
  object-colors.cpp
  object-hierarchy.cpp
  object-snapper.cpp
  page-manager.cpp
  path-chemistry.cpp
  path-prefix.cpp
  perspective-line.cpp
  preferences.cpp
  print.cpp
  proj_pt.cpp
  pure-transform.cpp
  rdf.cpp
  rubberband.cpp
  selcue.cpp
  selection-chemistry.cpp
  selection-describer.cpp
  selection.cpp
  seltrans-handles.cpp
  seltrans.cpp
  snap-preferences.cpp
  snap.cpp
  snapped-curve.cpp
  snapped-line.cpp
  snapped-point.cpp
  snapper.cpp
  style-internal.cpp
  style-text.cpp
  style.cpp
  text-chemistry.cpp
  text-editing.cpp
  transf_mat_3x4.cpp
  unicoderange.cpp
  vanishing-point.cpp
  version.cpp

  # Headers

  alignment-snapper.h
  attribute-rel-css.h
  attribute-rel-svg.h
  attribute-rel-util.h
  attribute-sort-util.h
  attributes.h
  auto-save.h
  axis-manip.h
  bad-uri-exception.h
  composite-undo-stack-observer.h
  conditions.h
  conn-avoid-ref.h
  context-fns.h
  css-chemistry.h
  desktop-events.h
  desktop-style.h
  desktop.h
  distribution-snapper.h
  document-subset.h
  document-undo.h
  document-update.h
  document.h
  enums.h
  event-log.h
  event.h
  file.h
  fill-or-stroke.h
  filter-chemistry.h
  filter-enums.h
  gc-anchored.h
  gradient-chemistry.h
  gradient-drag.h
  guide-snapper.h
  grid-snapper.h
  id-clash.h
  inkscape-version.h
  inkscape-version-info.h
  inkscape.h
  layer-manager.h
  line-geometry.h
  line-snapper.h
  media.h
  message-context.h
  message-stack.h
  message.h
  mod360.h
  number-opt-number.h
  object-colors.h
  object-hierarchy.h
  object-snapper.h
  page-manager.h
  path-chemistry.h
  path-prefix.h
  pattern-manager.cpp
  pattern-manager.h
  pattern-manipulation.cpp
  pattern-manipulation.h
  perspective-line.h
  preferences-skeleton.h
  preferences.h
  print.h
  proj_pt.h
  pure-transform.h
  rdf.h
  rubberband.h
  selcue.h
  selection-chemistry.h
  selection-describer.h
  selection.h
  seltrans-handles.h
  seltrans.h
  snap-candidate.h
  snap-enums.h
  snap-preferences.h
  snap.h
  snapped-curve.h
  snapped-line.h
  snapped-point.h
  snapper.h
  streq.h
  strneq.h
  style-enums.h
  style-internal.h
  style-text.h
  style.h
  syseq.h
  text-chemistry.h
  text-editing.h
  text-tag-attributes.h
  transf_mat_3x4.h
  undo-stack-observer.h
  unicoderange.h
  vanishing-point.h
  version.h

  linea-window.h
  linea-window.cpp

  linea-application.h
  linea-application.cpp

  main-menu.h
  main-menu.cpp


  manipulation/copy-resource.h
  manipulation/copy-resource.cpp
)

# -----------------------------------------------------------------------------
# Generate version file
# -----------------------------------------------------------------------------

# configure with values known at configure time
configure_file(inkscape-version.cpp.in ${CMAKE_BINARY_DIR}/src/inkscape-version.cpp.in @ONLY)

# configure further at build time (always built as run_always.txt is never actually created)
add_custom_command(
    OUTPUT inkscape-version.cpp run_always.txt
    COMMAND ${CMAKE_COMMAND}
        -DINKSCAPE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DINKSCAPE_BINARY_DIR=${CMAKE_BINARY_DIR}
        -P ${CMAKE_SOURCE_DIR}/CMakeScripts/inkscape-version.cmake
    COMMENT "Generating inkscape-version.cpp")

target_sources(linea_base PRIVATE
  ${CMAKE_BINARY_DIR}/src/inkscape-version.cpp
)


# -----------------------------------------------------------------------------
# Load in subdirectories
# -----------------------------------------------------------------------------

add_subdirectory(actions)
add_subdirectory(async)
add_subdirectory(colors)
add_subdirectory(css)
add_subdirectory(debug)
add_subdirectory(display)
add_subdirectory(extension)
add_subdirectory(helper)
add_subdirectory(include)
add_subdirectory(inkgc)
add_subdirectory(io)
add_subdirectory(libnrtype)
add_subdirectory(livarot)
add_subdirectory(live_effects)
add_subdirectory(object)
add_subdirectory(path)
add_subdirectory(props)
add_subdirectory(svg)
add_subdirectory(trace)
add_subdirectory(ui)
add_subdirectory(qt)
add_subdirectory(util)
add_subdirectory(util-string)
add_subdirectory(widgets)
add_subdirectory(xml)

# Directories containing lists files that describe building internal libraries
add_subdirectory(3rdparty)

# -----------------------------------------------------------------------------
# Relative RPATH handling
# -----------------------------------------------------------------------------

file(RELATIVE_PATH
    INKSCAPE_INSTALL_LIBDIR_RELATIVE_TO_BINDIR
    "${CMAKE_INSTALL_FULL_BINDIR}"
    "${CMAKE_INSTALL_FULL_LIBDIR}")

if(APPLE)
    set(CMAKE_MACOSX_RPATH TRUE)
    set(ORIGIN "@loader_path")
else()
    set(ORIGIN "$ORIGIN")
endif()

set(BINS_INSTALL_RPATH ${CMAKE_INSTALL_RPATH})
set(LIBINKSCAPE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH})

# Binaries must be able to find liblinea_base
list(APPEND BINS_INSTALL_RPATH "${ORIGIN}/${INKSCAPE_INSTALL_LIBDIR_RELATIVE_TO_BINDIR}/linea")

if(WITH_INTERNAL_2GEOM)
    # Binaries must be able to find lib2geom
    list(APPEND BINS_INSTALL_RPATH "${ORIGIN}/${INKSCAPE_INSTALL_LIBDIR_RELATIVE_TO_BINDIR}")
    # liblinea_base must be able to find lib2geom
    list(APPEND LIBINKSCAPE_INSTALL_RPATH "${ORIGIN}/..")
endif()



set_target_properties(linea_base PROPERTIES SOVERSION "${INKSCAPE_VERSION_MAJOR}.${INKSCAPE_VERSION_MINOR}.${INKSCAPE_VERSION_PATCH}.0")
set_target_properties(linea_base PROPERTIES INSTALL_RPATH "${LIBINKSCAPE_INSTALL_RPATH}")

if(WITH_CAPYPDF)
  if(TARGET capypdf)
    add_dependencies(linea_base capypdf)
  endif()
endif()

# Link the linea_base library against all external dependencies
target_link_libraries(linea_base
    PRIVATE
        autotrace_LIB
        Depixelize::depixelize
        croco_LIB
        adaptagrams_LIB
        Uemf::uemf
    PUBLIC
        ${INKSCAPE_LIBS}
        2Geom::2geom
)

# Qt6 properties on linea_base (sources are added from src/qt/CMakeLists.txt)
message(STATUS "Adding Qt6 sources to linea_base")

# Set Qt-specific properties
set_target_properties(linea_base PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)


# Process SVG icons for light/dark themes
find_package(Python3 REQUIRED)

# Get all SVG icon files for dependency tracking
file(GLOB ICON_SVG_FILES "${CMAKE_SOURCE_DIR}/share/icons/Dash/symbolic/actions/*.svg")

# Run Python script to generate themed icon sets
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/icons-light.qrc ${CMAKE_BINARY_DIR}/icons-dark.qrc
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/buildtools/process-icons.py
        --source ${CMAKE_SOURCE_DIR}/share/icons/Dash/symbolic/actions
        --build-dir ${CMAKE_BINARY_DIR}
    DEPENDS ${CMAKE_SOURCE_DIR}/buildtools/process-icons.py ${ICON_SVG_FILES}
    COMMENT "Processing icons for light/dark themes"
)

# Generate binary .rcc files from the themed qrc files
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/icons-light.rcc ${CMAKE_BINARY_DIR}/icons-dark.rcc
    COMMAND Qt6::rcc -binary ${CMAKE_BINARY_DIR}/icons-light.qrc -o ${CMAKE_BINARY_DIR}/icons-light.rcc
    COMMAND Qt6::rcc -binary ${CMAKE_BINARY_DIR}/icons-dark.qrc -o ${CMAKE_BINARY_DIR}/icons-dark.rcc
    DEPENDS ${CMAKE_BINARY_DIR}/icons-light.qrc ${CMAKE_BINARY_DIR}/icons-dark.qrc
    COMMENT "Compiling binary icon resource files"
)

# Create qrc file to embed the .rcc files in the binary
file(WRITE ${CMAKE_BINARY_DIR}/embedded-icons.qrc
    "<!DOCTYPE RCC>\n"
    "<RCC version=\"1.0\">\n"
    "    <qresource prefix=\"/\">\n"
    "        <file alias=\"icons-light.rcc\">icons-light.rcc</file>\n"
    "        <file alias=\"icons-dark.rcc\">icons-dark.rcc</file>\n"
    "    </qresource>\n"
    "</RCC>\n"
)

# Add the embedded .rcc resources to the binary
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/embedded-icons.cpp
    COMMAND Qt6::rcc ${CMAKE_BINARY_DIR}/embedded-icons.qrc -o ${CMAKE_BINARY_DIR}/embedded-icons.cpp
    DEPENDS ${CMAKE_BINARY_DIR}/embedded-icons.qrc ${CMAKE_BINARY_DIR}/icons-light.rcc ${CMAKE_BINARY_DIR}/icons-dark.rcc
    COMMENT "Compiling embedded icons resource"
)
target_sources(linea_base PRIVATE ${CMAKE_BINARY_DIR}/embedded-icons.cpp)

# Add the icon resources as dependencies
add_custom_target(themed_icons ALL DEPENDS ${CMAKE_BINARY_DIR}/embedded-icons.cpp)
add_dependencies(linea_base themed_icons)

# Add Qt resources
qt_add_resources(linea_qt_resources
    "${CMAKE_SOURCE_DIR}/share/ui/resources/styles.qrc"
    "${CMAKE_SOURCE_DIR}/share/keys/keys.qrc"
)
target_sources(linea_base PRIVATE ${linea_qt_resources})

# Add qt/ui to include path for UI-generated headers
target_include_directories(linea_base PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/qt/ui
)

# Qt definitions and link libraries for linea_base
target_compile_definitions(linea_base PRIVATE WITH_QT6 QT_NO_KEYWORDS QT_DISABLE_DEPRECATED_UP_TO=0x060B00)
target_link_libraries(linea_base PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Xml
)


if(BUILD_SHARED_LIBS AND NOT APPLE)
    install(TARGETS linea_base LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/linea")
endif()

# -----------------------------------------------------------------------------
# Linea executable
# -----------------------------------------------------------------------------

add_executable(linea qt/main.cpp)

target_compile_definitions(linea PRIVATE QT_NO_KEYWORDS)

target_include_directories(linea PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/qt/ui
)

# Link to linea_base (which has PUBLIC Qt6 linkage and AUTOMOC/AUTOUIC enabled)
target_link_libraries(linea PRIVATE
    linea_base
)

# Platform-specific settings
if(WIN32)
    set_target_properties(linea PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
    set_target_properties(linea PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Linea"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.linea-draw.Linea"
        # @rpath dependencies (liblinea_base, bundled brew libs) resolve via the
        # bundle's Frameworks directory at runtime and during macdeployqt.
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    # Our bundled libs live side by side in Frameworks: resolve each other
    # relative to their own location.
    set_target_properties(linea_base 2geom PROPERTIES
        INSTALL_RPATH "@loader_path"
    )

    # Symlink share/ into the app bundle for development (fast, no duplication).
    # Use install(DIRECTORY) below for the install/CPack tree.
    add_custom_command(TARGET linea POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/bin/linea.app/Contents/Resources/share"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_SOURCE_DIR}/share"
            "${CMAKE_BINARY_DIR}/bin/linea.app/Contents/Resources/share/linea"
        COMMENT "Linking share/ into app bundle"
    )
endif()

# Installation
if(APPLE)
    # Keep the development symlink out of the installed bundle.
    install(CODE "file(REMOVE \"${CMAKE_BINARY_DIR}/bin/linea.app/Contents/Resources/share/linea\")")
endif()
install(TARGETS linea
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
if(APPLE)
    # Bundle our own shared libraries so @rpath references resolve.
    install(TARGETS linea_base 2geom
        LIBRARY DESTINATION "linea.app/Contents/Frameworks"
    )
    # Remove the development symlink copied with the bundle before installing a real copy.
    install(CODE "file(REMOVE \"${CMAKE_INSTALL_PREFIX}/linea.app/Contents/Resources/share/linea\")")
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/share/"
    DESTINATION "linea.app/Contents/Resources/share/linea"
)
if(APPLE)
    # Restore the development symlink after the installed copy is complete.
    install(CODE "
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E make_directory
                \"${CMAKE_BINARY_DIR}/bin/linea.app/Contents/Resources/share\"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                \"${CMAKE_SOURCE_DIR}/share\"
                \"${CMAKE_BINARY_DIR}/bin/linea.app/Contents/Resources/share/linea\"
        )
    ")
endif()

# Bundle Qt frameworks and shared libraries into the .app at install time.
# Only runs during `cmake --install`, not on dev builds.
if(APPLE)
    get_filename_component(QT_DEPLOY_PREFIX "${QT_DEPLOY_TOOL}/.." ABSOLUTE)
    get_filename_component(QT_BREW_PREFIX "${QT_DEPLOY_PREFIX}/../.." ABSOLUTE)
    set(QT_DEPLOY_LIBPATH_ARGS)
    foreach(QT_PACKAGE qt qtbase qtsvg qtdeclarative qtvirtualkeyboard)
        set(QT_PACKAGE_LIBDIR "${QT_BREW_PREFIX}/opt/${QT_PACKAGE}/lib")
        if(IS_DIRECTORY "${QT_PACKAGE_LIBDIR}")
            list(APPEND QT_DEPLOY_LIBPATH_ARGS "-libpath=${QT_PACKAGE_LIBDIR}")
        endif()
    endforeach()

    install(CODE "
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env
                DYLD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib
                \"${QT_DEPLOY_TOOL}\" \"\${CMAKE_INSTALL_PREFIX}/linea.app\"
                -always-overwrite
                ${QT_DEPLOY_LIBPATH_ARGS}
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR \"macdeployqt failed with code \${result}\")
        endif()
    ")
endif()
