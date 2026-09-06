# SPDX-License-Identifier: GPL-2.0-or-later

set(libnrtype_SRC
	font-qt-bridge.cpp
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
	Layout-TNG-Scanline-Maker.h
	Layout-TNG.h
	style-attachments.h
)

include("${CMAKE_CURRENT_LIST_DIR}/CMakeUnit.txt")

target_sources(linea_base PRIVATE ${libnrtype_unit_SRC} ${libnrtype_SRC})
