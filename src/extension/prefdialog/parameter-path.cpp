// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Path parameter for extensions
 *//*
 * Authors:
 *   Patrick Storz <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2019 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-path.h"

#include <cstdlib>
#include <cstring>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <glibmm/regex.h>

#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QFileDialog>

#include "extension/extension.h"
#include "preferences.h"
#include "xml/node.h"

namespace Inkscape::Extension {

ParamPath::ParamPath(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // get value
    const char *value = nullptr;
    if (xml->firstChild()) {
        value = xml->firstChild()->content();
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getString(pref_name()).raw();

    if (_value.empty() && value) {
        _value = value;
    }

    // parse selection mode
    const char *mode = xml->attribute("mode");
    if (mode) {
        if (!strcmp(mode, "file")) {
            _mode = Mode::file;
        } else if (!strcmp(mode, "files")) {
            _mode = Mode::file;
            _select_multiple = true;
        } else if (!strcmp(mode, "folder")) {
            _mode = Mode::folder;
        } else if (!strcmp(mode, "folders")) {
            _mode = Mode::folder;
            _select_multiple = true;
        } else if (!strcmp(mode, "file_new")) {
            _mode = Mode::file_new;
        } else if (!strcmp(mode, "folder_new")) {
            _mode = Mode::folder_new;
        } else {
            g_warning("Invalid value ('%s') for mode of parameter '%s' in extension '%s'",
                      mode, _name, _extension->get_id());
        }
    }

    // parse filetypes
    const char *filetypes = xml->attribute("filetypes");
    if (filetypes) {
        _filetypes = Glib::Regex::split_simple("," , filetypes);
    }
}

/**
 * A function to set the \c _value.
 *
 * This function sets the internal value, but it also sets the value
 * in the preferences structure.  To put it in the right place \c pref_name() is used.
 *
 * @param  in   The value to set to.
 */
const std::string& ParamPath::set(const std::string &in)
{
    _value = in;

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setString(pref_name(), _value);

    return _value;
}

std::string ParamPath::value_to_string() const
{
    if (!Glib::path_is_absolute(_value) && !_value.empty()) {
        return Glib::build_filename(_extension->get_base_directory(), _value);
    } else {
        return _value;
    }
}

void ParamPath::string_to_value(const std::string &in)
{
    _value = in;
}

/**
 * Creates a text box for the string parameter.
 *
 * Builds a hbox with a label and a text box in it.
 */
QWidget* ParamPath::get_widget(sigc::signal<void ()>* changeSignal)
{
    if (_hidden) {
        return nullptr;
    }

    auto const hbox = new QWidget();
    auto const layout = new QHBoxLayout(hbox);
    layout->setSpacing(GUI_PARAM_WIDGETS_SPACING);
    layout->setContentsMargins(0, 0, 0, 0);

    auto const label = new QLabel(QString::fromUtf8(_text));
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(label, 0);

    auto const textbox = new QLineEdit();
    textbox->setText(QString::fromStdString(_value));
    layout->addWidget(textbox, 1);
    _entry = textbox;

    QObject::connect(textbox, &QLineEdit::textChanged, [this, changeSignal](QString const& data) {
        set(data.toStdString());
        if (changeSignal != nullptr) {
            changeSignal->emit();
        }
    });

    auto const button = new QPushButton(QStringLiteral("…"));
    layout->addWidget(button, 0);
    QObject::connect(button, &QPushButton::clicked, [this]() { on_button_clicked(); });

    return hbox;
}

/**
 * Create and show the file chooser dialog when the "…" button is clicked
 * Then set the value of the ParamPathEntry holding the current value accordingly
 */
void ParamPath::on_button_clicked()
{
    QString caption;
    if (_mode == Mode::file) {
        caption = _select_multiple ? QObject::tr("Select existing files")
                                   : QObject::tr("Select existing file");
    } else if (_mode == Mode::folder) {
        caption = _select_multiple ? QObject::tr("Select existing folders")
                                   : QObject::tr("Select existing folder");
    } else if (_mode == Mode::file_new) {
        caption = QObject::tr("Choose file name");
    } else if (_mode == Mode::folder_new) {
        caption = QObject::tr("Choose folder name");
    } else {
        return;
    }

    // build filter string from filetypes
    QString filter;
    if (!_filetypes.empty() && _mode != Mode::folder && _mode != Mode::folder_new) {
        std::vector<std::string> patterns;
        for (auto const& ft : _filetypes) {
            patterns.push_back("*." + ft.raw());
        }
        std::string joined = boost::algorithm::join(patterns, " ");
        std::string name = boost::algorithm::join(_filetypes, "+");
        boost::algorithm::to_upper(name);
        filter = QString::fromStdString(name + " (" + joined + ")");
    }

    // determine initial directory
    QString dir;
    if (!_value.empty()) {
        auto first_filename = _value.substr(0, _value.find("|"));
        if (!Glib::path_is_absolute(first_filename)) {
            first_filename = Glib::build_filename(_extension->get_base_directory(), first_filename);
        }
        dir = QString::fromStdString(Glib::path_get_dirname(first_filename));
    }

    QString result;
    if (_mode == Mode::folder || _mode == Mode::folder_new) {
        result = QFileDialog::getExistingDirectory(nullptr, caption, dir);
    } else if (_mode == Mode::file_new) {
        result = QFileDialog::getSaveFileName(nullptr, caption, dir, filter);
    } else {
        if (_select_multiple) {
            auto files = QFileDialog::getOpenFileNames(nullptr, caption, dir, filter);
            if (!files.isEmpty()) {
                QStringList list;
                for (auto const& f : files) list << f;
                result = list.join("|");
            }
        } else {
            result = QFileDialog::getOpenFileName(nullptr, caption, dir, filter);
        }
    }

    if (!result.isEmpty() && _entry) {
        _entry->setText(result);
    }
}

} // namespace Inkscape::Extension
