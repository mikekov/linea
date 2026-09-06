// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_TOGGLEBUTTON_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_TOGGLEBUTTON_H

/*
 * Copyright (C) Jabiertxo Arraiza Cenoz 2014
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glib.h>
#include <sigc++/signal.h>

#include "live_effects/parameter/parameter.h"

#include <QObject>

class QPushButton;

namespace Inkscape {

namespace LivePathEffect {

/**
 * class ToggleButtonParam:
 *    represents a toggle button as a Live Path Effect parameter
 */
class ToggleButtonParam : public Parameter {
public:
  ToggleButtonParam(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                    Inkscape::UI::Widget::Registry *wr, Effect *effect, bool default_value = false,
                    Glib::ustring inactive_label = "", char const *icon_active = nullptr,
                    char const *icon_inactive = nullptr);
  ~ToggleButtonParam() override;
  ToggleButtonParam(const ToggleButtonParam &) = delete;
  ToggleButtonParam &operator=(const ToggleButtonParam &) = delete;

  QWidget* param_newWidget() override;

  bool param_readSVGValue(const gchar *strvalue) override;
  Glib::ustring param_getSVGValue() const override;
  Glib::ustring param_getDefaultSVGValue() const override;

  void param_setValue(bool newvalue);
  void param_set_default() override;

  bool get_value() const { return value; };

  inline operator bool() const { return value; };

  sigc::signal<void ()> &signal_toggled() { return _signal_toggled; }
  virtual void toggled();
  void param_update_default(bool default_value);
  void param_update_default(const gchar *default_value) override;
  ParamType paramType() const override { return ParamType::TOGGLE_BUTTON; };
private:
    void refresh_button();
    void update_button_label();
    bool value;
    bool defvalue;
    const Glib::ustring inactive_label;
    const char * _icon_active;
    const char * _icon_inactive;
    QPushButton* checkwdg = nullptr;

    sigc::signal<void ()> _signal_toggled;
    QMetaObject::Connection _toggled_connection;
};


} //namespace LivePathEffect

} //namespace Inkscape

#endif
