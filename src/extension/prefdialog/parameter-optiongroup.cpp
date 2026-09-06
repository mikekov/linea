// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *extension parameter for options with multiple predefined value choices
 *
 * Currently implemented as either Gtk::CheckButton or Gtk::ComboBoxText
 */

/*
 * Author:
 *   Johan Engelen <johan@shouraizou.nl>
 *
 * Copyright (C) 2006-2007 Johan Engelen
 * Copyright (C) 2008 Jon A. Cruz
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-optiongroup.h"

#include <unordered_set>
#include <QComboBox>
#include <QRadioButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QButtonGroup>

#include "extension/extension.h"
#include "preferences.h"
#include "xml/node.h"

namespace Inkscape::Extension {

ParamOptionGroup::ParamOptionGroup(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : InxParameter(xml, ext)
{
    // Read valid optiongroup choices from XML tree, i,e.
    //   - <option> elements
    //   - <item> elements (for backwards-compatibility with params of type enum)
    //   - underscored variants of both (for backwards-compatibility)
    if (xml) {
        Inkscape::XML::Node *child_repr = xml->firstChild();
        while (child_repr) {
            const char *chname = child_repr->name();
            if (chname && (!strcmp(chname, INKSCAPE_EXTENSION_NS "option") ||
                           !strcmp(chname, INKSCAPE_EXTENSION_NS "_option") ||
                           !strcmp(chname, INKSCAPE_EXTENSION_NS "item") ||
                           !strcmp(chname, INKSCAPE_EXTENSION_NS "_item")) ) {
                child_repr->setAttribute("name", "option"); // TODO: hack to allow options to be parameters
                child_repr->setAttribute("gui-text", "option"); // TODO: hack to allow options to be parameters
                ParamOptionGroupOption *param = new ParamOptionGroupOption(child_repr, ext, this);
                choices.push_back(param);
            } else if (child_repr->type() == XML::NodeType::ELEMENT_NODE) {
                g_warning("Invalid child element ('%s') for parameter '%s' in extension '%s'. Expected 'option'.",
                          chname, _name, _extension->get_id());
            } else if (child_repr->type() != XML::NodeType::COMMENT_NODE){
                g_warning("Invalid child element found in parameter '%s' in extension '%s'. Expected 'option'.",
                          _name, _extension->get_id());
            }
            child_repr = child_repr->next();
        }
    }
    if (choices.empty()) {
        g_warning("No (valid) choices for parameter '%s' in extension '%s'", _name, _extension->get_id());
    }

    // check for duplicate option texts and values
    std::unordered_set<std::string> texts;
    std::unordered_set<std::string> values;
    for (auto choice : choices) {
        auto ret1 = texts.emplace(choice->_text.raw());
        if (!ret1.second) {
            g_warning("Duplicate option text ('%s') for parameter '%s' in extension '%s'.",
                      choice->_text.c_str(), _name, _extension->get_id());
        }
        auto ret2 = values.emplace(choice->_value.raw());
        if (!ret2.second) {
            g_warning("Duplicate option value ('%s') for parameter '%s' in extension '%s'.",
                      choice->_value.c_str(), _name, _extension->get_id());
        }
    }

    // get value (initialize with value of first choice if pref is empty)
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getString(pref_name());

    if (_value.empty()) {
        if (!choices.empty()) {
            _value = choices[0]->_value;
        }
        for (auto const * choice : choices) {
            if (choice->_is_default) {
                _value = choice->_value;
                break;
            }
        }
    }

    // parse appearance
    // (we support "combo" and "radio"; "minimal" is for backwards-compatibility)
    if (_appearance) {
        if (!strcmp(_appearance, "combo") || !strcmp(_appearance, "minimal")) {
            _mode = COMBOBOX;
        } else if (!strcmp(_appearance, "radio")) {
            _mode = RADIOBUTTON;
        } else {
            g_warning("Invalid value ('%s') for appearance of parameter '%s' in extension '%s'",
                      _appearance, _name, _extension->get_id());
        }
    }
}

ParamOptionGroup::~ParamOptionGroup ()
{
    // destroy choice strings
    for (auto choice : choices) {
        delete choice;
    }
}

/**
 * A function to set the \c _value.
 *
 * This function sets ONLY the internal value, but it also sets the value
 * in the preferences structure.  To put it in the right place \c pref_name() is used.
 *
 * @param  in   The value to set.
 */
const Glib::ustring &ParamOptionGroup::set(const Glib::ustring &in)
{
    if (contains(in)) {
        _value = in;
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->setString(pref_name(), _value.c_str());
    } else {
        g_warning("Could not set value ('%s') for parameter '%s' in extension '%s'. Not a valid choice.",
                  in.c_str(), _name, _extension->get_id());
    }

    return _value;
}

bool ParamOptionGroup::contains(const Glib::ustring text) const
{
    for (auto choice : choices) {
        if (choice->_value == text) {
            return true;
        }
    }

    return false;
}

std::string ParamOptionGroup::value_to_string() const
{
    return _value.raw();
}

void ParamOptionGroup::string_to_value(const std::string &in)
{
    _value = in;
}

/**
 * Returns the value for the options label parameter
 */
Glib::ustring ParamOptionGroup::value_from_label(const Glib::ustring label)
{
    Glib::ustring value;

    for (auto choice : choices) {
        if (choice->_text == label) {
            value = choice->_value;
            break;
        }
    }

    return value;
}

/**
 * Creates the widget for the optiongroup parameter.
 */
QWidget* ParamOptionGroup::get_widget(sigc::signal<void ()>* changeSignal) {
    if (_hidden) {
        return nullptr;
    }

    auto const hbox = new QWidget();
    auto const layout = new QHBoxLayout(hbox);
    layout->setSpacing(GUI_PARAM_WIDGETS_SPACING);
    layout->setContentsMargins(0, 0, 0, 0);

    auto const label = new QLabel(QString::fromUtf8(_text));
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(label, 1);

    if (_mode == COMBOBOX) {
        auto const combo = new QComboBox();

        int selected = 0;
        int idx = 0;
        for (auto choice : choices) {
            combo->addItem(QString::fromStdString(choice->_text.raw()));
            if (choice->_value == _value) {
                selected = idx;
            }
            ++idx;
        }
        combo->setCurrentIndex(selected);

        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         [this, combo, changeSignal](int) {
            QString text = combo->currentText();
            Glib::ustring value = value_from_label(Glib::ustring(text.toUtf8().constData()));
            set(value.c_str());
            if (changeSignal) {
                changeSignal->emit();
            }
        });

        layout->addWidget(combo, 0);
    } else if (_mode == RADIOBUTTON) {
        auto const radios = new QWidget();
        auto const radio_layout = new QVBoxLayout(radios);
        radio_layout->setContentsMargins(0, 0, 0, 0);
        radio_layout->setSpacing(GUI_PARAM_WIDGETS_SPACING);

        auto const group = new QButtonGroup(radios);

        for (auto choice : choices) {
            auto const radio = new QRadioButton(QString::fromStdString(choice->_text.raw()));
            if (choice->_value == _value) {
                radio->setChecked(true);
            }
            group->addButton(radio);
            radio_layout->addWidget(radio);

            QObject::connect(radio, &QRadioButton::toggled, [this, choice, changeSignal](bool checked) {
                if (checked) {
                    set(choice->_value.c_str());
                    if (changeSignal) {
                        changeSignal->emit();
                    }
                }
            });
        }

        layout->addWidget(radios, 0);
    }

    return hbox;
}

ParamOptionGroup::ParamOptionGroupOption::ParamOptionGroupOption(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext,
                                                                 const Inkscape::Extension::ParamOptionGroup *parent)
    : InxParameter(xml, ext)
{
    // get content (=label) of option and translate it
    const char *text = nullptr;
    if (xml->firstChild()) {
        text = xml->firstChild()->content();
    }
    if (text) {
        if (_translatable != NO) { // translate unless explicitly marked untranslatable
            _text = get_translation(text);
        } else {
            _text = text;
        }
    } else {
        g_warning("Missing content in option of parameter '%s' in extension '%s'.",
                  parent->_name, _extension->get_id());
    }

    // get string value of option
    const char *value = xml->attribute("value");
    if (value) {
        _value = value;
    } else {
        if (text) {
            const char *name = xml->name();
            if (!strcmp(name, INKSCAPE_EXTENSION_NS "item") || !strcmp(name, INKSCAPE_EXTENSION_NS "_item")) {
                _value = text; // use untranslated UI text as value (for backwards-compatibility)
            } else {
                _value = _text; // use translated UI text as value
            }
        } else {
            g_warning("Missing value for option '%s' of parameter '%s' in extension '%s'.",
                      _text.c_str(), parent->_name, _extension->get_id());
        }
    }

    _is_default = g_strcmp0(xml->attribute("default"), "true") == 0;
}

} // namespace Inkscape::Extension
