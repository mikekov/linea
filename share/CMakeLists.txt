# SPDX-License-Identifier: GPL-2.0-or-later
add_subdirectory(attributes)
add_subdirectory(branding)
add_subdirectory(examples)
add_subdirectory(filters)
add_subdirectory(fonts)
add_subdirectory(icons)
add_subdirectory(keys)
add_subdirectory(markers)
add_subdirectory(paint)
add_subdirectory(palettes)
add_subdirectory(symbols)
add_subdirectory(templates)
add_subdirectory(ui)
add_subdirectory(doc)


# extensions (git submodule!)
install(DIRECTORY extensions DESTINATION ${INKSCAPE_SHARE_INSTALL}
    COMPONENT extensions
    PATTERN ".git*" EXCLUDE
    PATTERN ".pylintrc" EXCLUDE
    PATTERN ".darglint" EXCLUDE
    PATTERN ".pre-commit-config.yaml" EXCLUDE
    PATTERN "tests" EXCLUDE
    PATTERN "MANIFEST.in" EXCLUDE
    PATTERN "pyproject.toml" EXCLUDE
    PATTERN "Makefile" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "tox.ini" EXCLUDE
    PATTERN "setup.cfg" EXCLUDE
    PATTERN "requirements.txt" EXCLUDE
    PATTERN "poetry.lock" EXCLUDE
    PATTERN "*.dox" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
    PATTERN "*.sh" EXCLUDE
    PATTERN "*.bat" EXCLUDE
    PATTERN "*.md" EXCLUDE
    PATTERN "print_win32_vector.*" EXCLUDE
    PATTERN "other/inkman" EXCLUDE
)

install(DIRECTORY extensions/other/inkman DESTINATION ${INKSCAPE_SHARE_INSTALL}/extensions/inkman/
    COMPONENT extension_manager
    PATTERN "tests" EXCLUDE
    PATTERN ".git*" EXCLUDE
    PATTERN "MANIFEST.in" EXCLUDE
    PATTERN "requirements.txt" EXCLUDE
    PATTERN "pyproject.toml" EXCLUDE
)

if(WIN32)
    install(
        FILES extensions/print_win32_vector.py extensions/print_win32_vector.inx
        DESTINATION ${INKSCAPE_SHARE_INSTALL}/extensions
        COMPONENT extensions
    )
  install(CODE
    "MESSAGE(\"Pre-compiling Python extensions to byte-code (.pyc files)\")
     execute_process(COMMAND \${CMAKE_INSTALL_PREFIX}/bin/python -m compileall -qq \${CMAKE_INSTALL_PREFIX}/${INKSCAPE_SHARE_INSTALL})"
    COMPONENT extensions)
endif()


# themes (git submodule!)
# add_subdirectory(themes)


