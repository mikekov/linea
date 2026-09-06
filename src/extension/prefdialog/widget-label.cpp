// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Description widget for extensions
 *//*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2005-2019 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "widget-label.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QString>

#include <glibmm/regex.h>

#include "extension/extension.h"
#include "xml/node.h"

namespace Inkscape::Extension {

WidgetLabel::WidgetLabel(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxWidget(xml, ext)
{
    // construct the text content by concatenating all (non-empty) text nodes,
    // removing all other nodes (e.g. comment nodes) and replacing <extension:br> elements with "<br/>"
    Inkscape::XML::Node * cur_child = xml->firstChild();
    while (cur_child != nullptr) {
        if (cur_child->type() == XML::NodeType::TEXT_NODE && cur_child->content() != nullptr) {
            _value += cur_child->content();
        } else if (cur_child->type() == XML::NodeType::ELEMENT_NODE && !g_strcmp0(cur_child->name(), "extension:br")) {
            _value += "<br/>";
        }
        cur_child = cur_child->next();
    }

    // do replacements in the source string to account for the attribute xml:space="preserve"
    // (those should match replacements potentially performed by xgettext to allow for proper translation)
    if (g_strcmp0(xml->attribute("xml:space"), "preserve") == 0) {
        // xgettext copies the source string verbatim in this case, so no changes needed
    } else {
        // remove all whitespace from start/end of string and replace intermediate whitespace with a single space
        _value = Glib::Regex::create("^\\s+|\\s+$")->replace_literal(_value, 0, "", (Glib::Regex::MatchFlags)0);
        _value = Glib::Regex::create("\\s+")->replace_literal(_value, 0, " ", (Glib::Regex::MatchFlags)0);
    }

    // translate value
    if (!_value.empty()) {
        if (_translatable != NO) { // translate unless explicitly marked untranslatable
            _value = get_translation(_value.c_str());
        }
    }

    // finally replace all remaining <br/> with a real newline character
    _value = Glib::Regex::create("<br/>")->replace_literal(_value, 0, "\n", (Glib::Regex::MatchFlags)0);

    // parse appearance
    if (_appearance) {
        if (!strcmp(_appearance, "header")) {
            _mode = HEADER;
        } else if (!strcmp(_appearance, "url")) {
            _mode = URL;
        } else {
            g_warning("Invalid value ('%s') for appearance of label widget in extension '%s'",
                      _appearance, _extension->get_id());
        }
    }
}

/** \brief  Create a label for the description */
QWidget* WidgetLabel::get_widget(sigc::signal<void ()>* /*changeSignal*/) {
    if (_hidden) {
        return nullptr;
    }

    QString newtext = QString::fromStdString(_value);

    auto const label = new QLabel();
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    if (_mode == HEADER) {
        label->setTextFormat(Qt::RichText);
        QString escaped = newtext.toHtmlEscaped();
        label->setText("<b>" + escaped + "</b>");
        label->setContentsMargins(0, 5, 0, 5);
    } else if (_mode == URL) {
        label->setTextFormat(Qt::RichText);
        QString escaped = newtext.toHtmlEscaped();
        label->setText(QString("<a href='%1'>%1</a>").arg(escaped));
        label->setOpenExternalLinks(true);
    } else {
        label->setTextFormat(Qt::PlainText);
        label->setText(newtext);
    }

    // Limit width so long labels don't make the popup grow ridiculously wide.
    int len = newtext.length();
    if (len > GUI_MAX_LINE_LENGTH) {
        label->setMaximumWidth(GUI_MAX_LINE_LENGTH * 8); // approximate char width
    }

    auto const hbox = new QWidget();
    auto const layout = new QHBoxLayout(hbox);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label);
    return hbox;
}

} // namespace Inkscape::Extension
