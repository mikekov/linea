// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INK_ACTIONS_FILE_WINDOW_H
#define INK_ACTIONS_FILE_WINDOW_H

class LineaApplication;
class LineaWindow;

void document_new(LineaWindow* win);
void document_open(LineaWindow* win);
void document_save(LineaWindow* win);

void add_actions_file_window(LineaApplication* app);

#endif // INK_ACTIONS_FILE_WINDOW_H
