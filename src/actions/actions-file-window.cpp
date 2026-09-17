// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for opening, saving, etc. files which (mostly) open a dialog or an Inkscape window.
 *  Used by menu items under the "File" submenu.
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <giomm.h>
#include <glibmm/i18n.h>

#include "actions-file-window.h"
#include "actions-helper.h"
#include "action-registry.h"

#include "actions/action-meta.h"
#include "linea-application.h"
#include "linea-window.h"
#include "desktop.h"
#include "document.h"
#include "document-undo.h"
#include "file.h"
#include "print.h"
#include "preferences.h"
#include "qt/ui/dialog/choose-file.h"
// #include "ui/dialog/save-template-dialog.h"
// #include "ui/dialog/new-from-template.h"
#include "ui/icon-names.h"

void
document_new(LineaWindow* win)
{
    LINEA_APP.createNewDocument();
}

void document_new_from_template(LineaWindow* wnd, int template_index) {
    LINEA_APP.createNewDocument(template_index);
}

void
document_dialog_templates(LineaWindow* win)
{
    if (win) {
        //QT TODO
        // Inkscape::UI::NewFromTemplate::load_new_from_template(*win);
    }
}

void document_open(LineaWindow* win) {
    auto files = Inkscape::choose_file_open_images(_("Select file(s) to open"), win,
                                                   "/dialog/open/path", _("Open"));
    for (const auto& file : files) {
        LINEA_APP.openDocument(file);
    }
}

void
document_revert(LineaWindow* win)
{
    sp_file_revert_dialog();
}

void
document_save(LineaWindow* win)
{
    // Save File
    sp_file_save(*win, nullptr, nullptr);
}

void
document_save_as(LineaWindow* win)
{
    // Save File As
    sp_file_save_as(*win, nullptr, nullptr);
}

void
document_save_copy(LineaWindow* win)
{
    // Save A copy
    //QT TODO
    // sp_file_save_a_copy(*win, nullptr, nullptr);
}

void
document_save_template(LineaWindow* win)
{
    // Save As Template
    //QT TODO
    // Inkscape::UI::Dialog::SaveTemplate::save_document_as_template(*win);
}

void document_import(LineaWindow* win) {
    auto files = Inkscape::choose_file_open_images(_("Select file(s) to import"), win,
                                                   "/dialog/import/path", _("Import"));
    auto document = win->get_document();
    for (const auto& file : files) {
        file_import(document, file->get_path(), nullptr);
    }
}

void
document_print(LineaWindow* win)
{
    // Print File
    //QT NOT TODO
    // if (auto doc = win->get_document()) {
    //     sp_print_document(*win, doc);
    // }
}

void
document_cleanup(LineaWindow* win)
{
    // Cleanup Up Document
    auto doc = win->get_document();
    unsigned int diff = doc->vacuumDocument();

    Inkscape::DocumentUndo::done(doc, RC_("Undo", "Clean up document"), INKSCAPE_ICON("document-cleanup"));

    // Show status messages when in GUI mode
    if (diff > 0) {
        win->get_desktop()->messageStack()->flashF(Inkscape::NORMAL_MESSAGE,
                ngettext("Removed <b>%i</b> unused definition in &lt;defs&gt;.",
                        "Removed <b>%i</b> unused definitions in &lt;defs&gt;.",
                        diff),
                diff);
    } else {
        win->get_desktop()->messageStack()->flash(Inkscape::NORMAL_MESSAGE,  _("No unused definitions in &lt;defs&gt;."));
    }
}

// Close tab, checking for data loss.
void document_close(LineaWindow* win) {
    auto& app = LineaApplication::instance();
    if (auto desktop = win->get_desktop()) {
        app.destroyDesktop(desktop, false);
    }
}

const Glib::ustring SECTION = NC_("Action Section", "Window-File");

static auto file_window_action_defs = std::to_array<ActionSpec<LineaWindow>>({
    // clang-format off
    {"document-new",              N_("New"),               SECTION,   N_("Create new document from the default template"), "document-new", document_new},
    {"document-dialog-templates", N_("New from Template"), SECTION,   N_("Create new project from template"), nullptr, document_dialog_templates},
    {"document-open",             N_("Open…"),             SECTION,   N_("Open an existing document"), "document-open", document_open},
    {"document-revert",           N_("Revert"),            SECTION,   N_("Revert to the last saved version of document (changes will be lost)"), "document-revert", document_revert},
    {"document-save",             N_("Save"),              SECTION,   N_("Save document"), "document-save", document_save},
    {"document-save-as",          N_("Save As…"),          SECTION,   N_("Save document under a new name"), "document-save-as", document_save_as},
    {"document-save-copy",        N_("Save a Copy"),       SECTION,   N_("Save a copy of the document under a new name"), nullptr, document_save_copy},
    {"document-save-template",    N_("Save Template"),     SECTION,   N_("Save a copy of the document as template"), nullptr, document_save_template},
    {"document-import",           N_("Import…"),            SECTION,   N_("Import a bitmap or SVG image into this document"), "document-import", document_import},
    {"document-print",            N_("Print"),             SECTION,   N_("Print document"), "document-print", document_print},
    {"document-cleanup",          N_("Clean Up Document"), SECTION,   N_("Remove unused definitions (such as gradients or clipping paths) from the document"), "document-cleanup", document_cleanup},
    {"document-close",            N_("Close"),             SECTION,   N_("Close document (unless last document)"), nullptr, document_close},

    {"document-new-from-template-1", N_("New from Template 1"), SECTION, N_("Create new document from template 1"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 1);}},
    {"document-new-from-template-2", N_("New from template 2"), SECTION, N_("Create new document from template 2"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 2);}},
    {"document-new-from-template-3", N_("New from template 3"), SECTION, N_("Create new document from template 3"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 3);}},
    {"document-new-from-template-4", N_("New from template 4"), SECTION, N_("Create new document from template 4"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 4);}},
    {"document-new-from-template-5", N_("New from template 5"), SECTION, N_("Create new document from template 5"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 5);}},
    {"document-new-from-template-6", N_("New from template 6"), SECTION, N_("Create new document from template 6"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 6);}},
    {"document-new-from-template-7", N_("New from template 7"), SECTION, N_("Create new document from template 7"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 7);}},
    {"document-new-from-template-8", N_("New from template 8"), SECTION, N_("Create new document from template 8"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 8);}},
    {"document-new-from-template-9", N_("New from template 9"), SECTION, N_("Create new document from template 9"), nullptr, [](LineaWindow* wnd){document_new_from_template(wnd, 9);}},
    // clang-format on
});

void add_actions_file_window(LineaApplication* app) {
    ActionRegistry::get().registerActions(app, file_window_action_defs);
#if 0
    // clang-format off
    win->add_action( "document-new",                sigc::bind(sigc::ptr_fun(&document_new),               win));
    win->add_action( "document-dialog-templates",   sigc::bind(sigc::ptr_fun(&document_dialog_templates),  win));
    win->add_action( "document-open",               sigc::bind(sigc::ptr_fun(&document_open),              win));
    win->add_action( "document-revert",             sigc::bind(sigc::ptr_fun(&document_revert),            win));
    win->add_action( "document-save",               sigc::bind(sigc::ptr_fun(&document_save),              win));
    win->add_action( "document-save-as",            sigc::bind(sigc::ptr_fun(&document_save_as),           win));
    win->add_action( "document-save-copy",          sigc::bind(sigc::ptr_fun(&document_save_copy),         win));
    win->add_action( "document-save-template",      sigc::bind(sigc::ptr_fun(&document_save_template),     win));
    win->add_action( "document-import",             sigc::bind(sigc::ptr_fun(&document_import),            win));
    win->add_action( "document-print",              sigc::bind(sigc::ptr_fun(&document_print),             win));
    win->add_action( "document-cleanup",            sigc::bind(sigc::ptr_fun(&document_cleanup),           win));
    win->add_action( "document-close",              sigc::bind(sigc::ptr_fun(&document_close),             win));
    // clang-format on

    auto& app = LineaApplication::instance();
    if (!app) {
        show_output("add_actions_file_window: no app!");
        return;
    }
    app->get_action_extra_data().add_data(raw_data_dialog_window);
#endif
}
