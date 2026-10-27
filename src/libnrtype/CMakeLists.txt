# SPDX-License-Identifier: GPL-2.0-or-later

set(libnrtype_SRC
	font-qt-bridge.cpp
	font-utils.cpp
	Layout-TNG.cpp
	Layout-TNG-Compute.cpp
	Layout-TNG-Input.cpp
	Layout-TNG-OutIter.cpp
	Layout-TNG-Output.cpp
	Layout-TNG-Scanline-Makers.cpp
	style-attachments.cpp

	# -------
	# Headers
	font-qt-bridge.h
	font-utils.h
	Layout-TNG-Scanline-Maker.h
	Layout-TNG.h
	style-attachments.h
)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeUnit.txt")

target_sources(linea_base PRIVATE ${libnrtype_unit_SRC} ${libnrtype_SRC})

pkg_check_modules(HARFBUZZ_CAIRO IMPORTED_TARGET GLOBAL harfbuzz-cairo)
if (HARFBUZZ_CAIRO_FOUND)
	target_compile_definitions(linea_base PRIVATE WITH_HARFBUZZ_CAIRO)
	target_link_libraries(linea_base PRIVATE PkgConfig::HARFBUZZ_CAIRO)
endif()
