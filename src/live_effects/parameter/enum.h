// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_ENUM_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_ENUM_H

#include <glibmm/ustring.h>

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>

#include "live_effects/effect.h"
#include "live_effects/parameter/parameter.h"
#include "ui/icon-names.h"
#include "util/enums.h"

namespace Inkscape::LivePathEffect {

template<typename E> class EnumParam : public Parameter {
public:
    EnumParam(  const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key,
                const Util::EnumDataConverter<E>& c,
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect,
                E default_value,
                bool sort = true)
        : Parameter(label, tip, key, wr, effect)
    {
        enumdataconv = &c;
        defvalue = default_value;
        value = defvalue;
        sorted = sort;
    };

    EnumParam(const EnumParam&) = delete;
    EnumParam& operator=(const EnumParam&) = delete;

    QWidget* param_newWidget() override {
        auto combo = new QComboBox();
        int current_index = 0;
        for (unsigned int i = 0; i < enumdataconv->_length; ++i) {
            auto const& entry = enumdataconv->data(i);
            combo->addItem(QString::fromStdString(entry.label.raw()),
                           QString::fromStdString(entry.key.raw()));
            if (entry.id == value) {
                current_index = i;
            }
        }
        combo->setCurrentIndex(current_index);

        QObject::connect(combo, &QComboBox::activated, [this, combo](int) {
            auto key = combo->currentData().toString().toStdString();
            param_set_value(enumdataconv->get_id_from_key(Glib::ustring(key)));
            param_write_to_repr(key.c_str());
            param_effect->refresh_widgets = true;
        });

        return combo;
    };

    void _on_change_combo() { param_effect->refresh_widgets = true; }

    bool param_readSVGValue(const gchar * strvalue) override {
        if (!strvalue) {
            param_set_default();
            return true;
        }

        param_set_value( enumdataconv->get_id_from_key(Glib::ustring(strvalue)) );

        return true;
    };
    Glib::ustring param_getSVGValue() const override {
        return enumdataconv->get_key(value);
    };
    
    Glib::ustring param_getDefaultSVGValue() const override {
        return enumdataconv->get_key(defvalue).c_str();
    };
    
    E get_value() const {
        return value;
    }

    inline operator E() const {
        return value;
    };

    void param_set_default() override {
        param_set_value(defvalue);
    }
    
    void param_update_default(E default_value) {
        defvalue = default_value;
    }
    
    void param_update_default(const gchar * default_value) override {
        param_update_default(enumdataconv->get_id_from_key(Glib::ustring(default_value)));
    }
    
    void param_set_value(E val) {
        value = val;
    }
    ParamType paramType() const override { return ParamType::ENUM; };
private:
    E value;
    E defvalue;
    bool sorted;

    const Util::EnumDataConverter<E> * enumdataconv;
};

}; // namespace Inkscape::LivePathEffect

#endif // INKSCAPE_LIVEPATHEFFECT_PARAMETER_ENUM_H
