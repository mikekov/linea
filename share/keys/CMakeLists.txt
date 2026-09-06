# SPDX-License-Identifier: GPL-2.0-or-later
file(GLOB _FILES "*.xml")
install(FILES ${_FILES} DESTINATION ${INKSCAPE_SHARE_INSTALL}/keys)

# install a duplicate of linea.xml as default.xml
#   default.xml is the default keymap that will be used by linea after installation
#   replace this with a keymap of your own to override the Linea default shortcuts
install(FILES linea.xml
    DESTINATION ${INKSCAPE_SHARE_INSTALL}/keys
    RENAME default.xml)
