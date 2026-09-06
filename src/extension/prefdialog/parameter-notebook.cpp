// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Notebook and NotebookPage parameters for extensions.
 */
/*
 * Authors:
 *   Johan Engelen <johan@shouraizou.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2006 Author
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-notebook.h"

#include <unordered_set>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "extension/extension.h"
#include "preferences.h"
#include "xml/node.h"

namespace Inkscape {
namespace Extension {

ParamNotebook::ParamNotebookPage::ParamNotebookPage(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // Read XML tree of page and parse parameters
    if (xml) {
        Inkscape::XML::Node *child_repr = xml->firstChild();
        while (child_repr) {
            const char *chname = child_repr->name();
            if (!strncmp(chname, INKSCAPE_EXTENSION_NS_NC, strlen(INKSCAPE_EXTENSION_NS_NC))) {
                chname += strlen(INKSCAPE_EXTENSION_NS);
            }
            if (chname[0] == '_') { // allow leading underscore in tag names for backwards-compatibility
                chname++;
            }

            if (InxWidget::is_valid_widget_name(chname)) {
                InxWidget *widget = InxWidget::make(child_repr, _extension);
                if (widget) {
                    _children.push_back(widget);
                }
            } else if (child_repr->type() == XML::NodeType::ELEMENT_NODE) {
                g_warning("Invalid child element ('%s') in notebook page in extension '%s'.",
                          chname, _extension->get_id());
            } else if (child_repr->type() != XML::NodeType::COMMENT_NODE){
                g_warning("Invalid child element found in notebook page in extension '%s'.", _extension->get_id());
            }

            child_repr = child_repr->next();
        }
    }
}


/**
 * Creates a notebookpage widget for a notebook.
 *
 * Builds a notebook page (a vbox) and puts parameters on it.
 */
QWidget* ParamNotebook::ParamNotebookPage::get_widget(sigc::signal<void ()>* changeSignal)
{
    if (_hidden) {
        return nullptr;
    }

    auto const vbox = new QWidget();
    auto const layout = new QVBoxLayout(vbox);
    layout->setContentsMargins(GUI_BOX_MARGIN, GUI_BOX_MARGIN, GUI_BOX_MARGIN, GUI_BOX_MARGIN);
    layout->setSpacing(GUI_BOX_SPACING);

    // add parameters onto page (if any)
    for (auto child : _children) {
        QWidget* child_widget = child->get_widget(changeSignal);
        if (child_widget) {
            int indent = child->get_indent();
            child_widget->setContentsMargins(indent * GUI_INDENTATION, 0, 0, 0);
            layout->addWidget(child_widget);

            const char* tooltip = child->get_tooltip();
            if (tooltip) {
                child_widget->setToolTip(QString::fromUtf8(tooltip));
            }
        }
    }

    return vbox;
}

/** End ParamNotebookPage **/



/** ParamNotebook **/

ParamNotebook::ParamNotebook(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // Read XML tree to add pages (allow _page for backwards compatibility)
    if (xml) {
        Inkscape::XML::Node *child_repr = xml->firstChild();
        while (child_repr) {
            const char *chname = child_repr->name();
            if (chname && (!strcmp(chname, INKSCAPE_EXTENSION_NS "page") ||
                           !strcmp(chname, INKSCAPE_EXTENSION_NS "_page") )) {
                ParamNotebookPage *page;
                page = new ParamNotebookPage(child_repr, ext);

                if (page) {
                    _children.push_back(page);
                }
            } else if (child_repr->type() == XML::NodeType::ELEMENT_NODE) {
                g_warning("Invalid child element ('%s') for parameter '%s' in extension '%s'. Expected 'page'.",
                          chname, _name, _extension->get_id());
            } else if (child_repr->type() != XML::NodeType::COMMENT_NODE){
                g_warning("Invalid child element found in parameter '%s' in extension '%s'. Expected 'page'.",
                          _name, _extension->get_id());
            }
            child_repr = child_repr->next();
        }
    }
    if (_children.empty()) {
        g_warning("No (valid) pages for parameter '%s' in extension '%s'", _name, _extension->get_id());
    }

    // check for duplicate page names
    std::unordered_set<std::string> names;
    for (auto child : _children) {
        ParamNotebookPage *page = static_cast<ParamNotebookPage *>(child);
        auto ret = names.emplace(page->_name);
        if (!ret.second) {
            g_warning("Duplicate page name ('%s') for parameter '%s' in extension '%s'.",
                      page->_name, _name, _extension->get_id());
        }
    }

    // get value (initialize with value of first page if pref is empty)
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getString(pref_name());

    if (_value.empty()) {
        if (!_children.empty()) {
            ParamNotebookPage *first_page = dynamic_cast<ParamNotebookPage *>(_children[0]);
            _value = first_page->_name;
        }
    }
}


/**
 * A function to set the \c _value.
 *
 * This function sets the internal value, but it also sets the value
 * in the preferences structure.  To put it in the right place \c pref_name() is used.
 *
 * @param  in   The number of the page to set as new value.
 */
const Glib::ustring& ParamNotebook::set(const int in)
{
    int i = in < _children.size() ? in : _children.size()-1;
    ParamNotebookPage *page = dynamic_cast<ParamNotebookPage *>(_children[i]);

    if (page) {
        _value = page->_name;

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->setString(pref_name(), _value);
    }

    return _value;
}

std::string ParamNotebook::value_to_string() const
{
    return _value.raw();
}

void ParamNotebook::string_to_value(const std::string &in)
{
    _value = in;
}

/**
 * Creates a Notebook widget for a notebook parameter.
 *
 * Builds a notebook and puts pages in it.
 */
QWidget* ParamNotebook::get_widget(sigc::signal<void ()>* changeSignal)
{
    if (_hidden) {
        return nullptr;
    }

    auto const notebook = new QTabWidget();

    // add pages (if any) and switch to previously selected page
    int current_page = -1;
    int selected_page = -1;
    for (auto child : _children) {
        ParamNotebookPage* page = dynamic_cast<ParamNotebookPage*>(child);
        g_assert(child); // A ParamNotebook has only children of type ParamNotebookPage.
                         // If we receive a non-page child here something is very wrong!
        current_page++;

        QWidget* page_widget = page->get_widget(changeSignal);

        Glib::ustring page_text = page->_text;
        if (page->_translatable != NO) { // translate unless explicitly marked untranslatable
            page_text = page->get_translation(page_text.c_str());
        }

        notebook->addTab(page_widget, QString::fromStdString(page_text.raw()));

        if (_value == page->_name) {
            selected_page = current_page;
        }
    }
    if (selected_page >= 0) {
        notebook->setCurrentIndex(selected_page);
    }

    QObject::connect(notebook, &QTabWidget::currentChanged, [this, notebook](int pagenum) {
        if (notebook->isVisible()) {
            set(pagenum);
        }
    });

    return notebook;
}


}  // namespace Extension
}  // namespace Inkscape
