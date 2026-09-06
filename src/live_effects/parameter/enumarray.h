// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::LivePathEffectParameters
 *
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_ENUMARRAY_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_ENUMARRAY_H

#include <glib.h>

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QWidget>

#include "live_effects/lpeobject.h"
#include "live_effects/effect.h"
#include "live_effects/parameter/array.h"
#include "live_effects/parameter/parameter.h"
#include "util/enums.h"

namespace Inkscape {

namespace LivePathEffect {
typedef unsigned E;
class EnumArrayParam : public ArrayParam<Glib::ustring> {
public:
    EnumArrayParam( const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key,
                const Util::EnumDataConverter<E>& c,
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect,
                E default_value,
                bool visible = true,
                size_t n = 0,
                bool sort = true)
    : ArrayParam<Glib::ustring>(label, tip, key, wr, effect, n)
    , defvalue(default_value)
    {
        enumdataconv = &c;

        sorted = sort;
        widget_is_visible = visible;
    }

    ~EnumArrayParam() override = default;

    QWidget* param_newWidget() override {
        if (!widget_is_visible || !valid_index(_active_index)) {
            return nullptr;
        }

        auto combo = new QComboBox();
        Glib::ustring current_key = _vector[_active_index];
        int current_index = 0;
        for (unsigned int i = 0; i < enumdataconv->_length; ++i) {
            auto const& entry = enumdataconv->data(i);
            combo->addItem(QString::fromStdString(entry.label.raw()),
                           QString::fromStdString(entry.key.raw()));
            if (entry.key == current_key) {
                current_index = i;
            }
        }
        combo->setCurrentIndex(current_index);

        QObject::connect(combo, &QComboBox::activated, [this, combo](int) {
            auto key = combo->currentData().toString().toStdString();
            if (key.empty()) return;
            _vector[_active_index] = Glib::ustring(key);
            param_set_and_write_new_value(_vector);
        });

        return combo;
    };

    void _on_change_combo() {
        param_effect->refresh_widgets = true;
    }

    void param_setActive(size_t index) {
        _active_index = index;
        param_effect->refresh_widgets = true;
    }

    Glib::ustring param_getDefaultSVGValue() const override {
        return enumdataconv->get_key(defvalue).c_str();
    };

    void param_set_default() override {
        for (auto &vec : _vector) {
            vec = enumdataconv->get_key(defvalue).c_str();
        }
    };

    void param_update_default(E default_value) { 
        defvalue = default_value; 
    };

    void param_update_default(const gchar *default_value) override {
        param_update_default(enumdataconv->get_id_from_key(Glib::ustring(default_value)));
    }

    ParamType paramType() const override { return ParamType::ENUM_ARRAY; };

protected:
    friend class LPETaperStroke;

private:
    size_t _active_index = 0;
    E defvalue;
    bool sorted;
    const Util::EnumDataConverter<E> * enumdataconv;
    EnumArrayParam(const EnumArrayParam &) = delete;
    EnumArrayParam &operator=(const EnumArrayParam &) = delete;
};


} // namespace LivePathEffect

} // namespace Inkscape

#endif // INKSCAPE_LIVEPATHEFFECT_PARAMETER_ENUMARRAY_H
