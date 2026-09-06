// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl> *
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-string.h"

#include <utility>
#include <glibmm/regex.h>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QScrollArea>

#include "extension/extension.h"
#include "preferences.h"
#include "xml/node.h"

namespace Inkscape::Extension {

ParamString::ParamString(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // get value
    const char *value = nullptr;
    if (xml->firstChild()) {
        value = xml->firstChild()->content();
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getString(pref_name());

    if (_value.empty() && value) {
        _value = value;
    }

    // translate value
    if (!_value.empty()) {
        if (_translatable == YES) { // translate only if explicitly marked translatable
            _value = get_translation(_value.c_str());
        }
    }

    // max-length
    const char *max_length = xml->attribute("max-length");
    if (!max_length) {
        max_length = xml->attribute("max_length"); // backwards-compatibility with old name (underscore)
    }
    if (max_length) {
        _max_length = strtoul(max_length, nullptr, 0);
    }

    // parse appearance
    if (_appearance) {
        if (!strcmp(_appearance, "multiline")) {
            _mode = MULTILINE;
        } else {
            g_warning("Invalid value ('%s') for appearance of parameter '%s' in extension '%s'",
                      _appearance, _name, _extension->get_id());
        }
    }
}

/**
 * A function to set the \c _value.
 * This function sets the internal value, but it also sets the value
 * in the preferences structure.  To put it in the right place \c pref_name() is used.
 * @param  in   The value to set to.
 * @returns The same value, moved to our _value member.
 */
const Glib::ustring& ParamString::set(Glib::ustring in)
{
    _value = std::move(in);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setString(pref_name(), _value);
    return _value;
}

std::string ParamString::value_to_string() const
{
    return _value.raw();
}

void ParamString::string_to_value(const std::string &in)
{
    _value = in;
}

/**
 * Creates a text box for the string parameter.
 *
 * Builds a hbox with a label and a text box in it.
 */
QWidget* ParamString::get_widget(sigc::signal<void ()>* changeSignal) {
    if (_hidden) {
        return nullptr;
    }

    auto const box = new QWidget();
    QBoxLayout* layout;
    if (_mode == MULTILINE) {
        layout = new QVBoxLayout(box);
    } else {
        layout = new QHBoxLayout(box);
    }
    layout->setSpacing(GUI_PARAM_WIDGETS_SPACING);
    layout->setContentsMargins(0, 0, 0, 0);

    auto const label = new QLabel(QString::fromUtf8(_text));
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(label, 0);

    if (_mode == MULTILINE) {
        auto const entry = new QTextEdit();
        // replace literal '\n' with actual newlines for multiline strings
        Glib::ustring value = Glib::Regex::create("\\\\n")->replace_literal(this->get(), 0, "\n", (Glib::Regex::MatchFlags)0);
        entry->setPlainText(QString::fromStdString(value.raw()));
        entry->setAcceptRichText(false);

        QObject::connect(entry, &QTextEdit::textChanged, [this, entry, changeSignal]() {
            QString data = entry->toPlainText();
            // always store newlines as literal '\n'
            data.replace('\n', "\\n");
            set(Glib::ustring(data.toUtf8().constData()));
            if (changeSignal != nullptr) {
                changeSignal->emit();
            }
        });

        layout->addWidget(entry, 1);
    } else {
        auto const entry = new QLineEdit();
        entry->setText(QString::fromStdString(_value.raw()));
        if (_max_length > 0) {
            entry->setMaxLength(_max_length);
        }

        QObject::connect(entry, &QLineEdit::textChanged, [this, entry, changeSignal](QString const& data) {
            set(Glib::ustring(data.toUtf8().constData()));
            if (changeSignal != nullptr) {
                changeSignal->emit();
            }
        });

        layout->addWidget(entry, 1);
    }

    return box;
}

} // namespace Inkscape::Extension
