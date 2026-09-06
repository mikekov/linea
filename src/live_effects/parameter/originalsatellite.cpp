// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2012 <j.b.c.engelen@alumnus.utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/originalsatellite.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <glibmm/i18n.h>

#include "desktop.h"
#include "display/curve.h"
#include "inkscape.h"
#include "live_effects/effect.h"
#include "live_effects/parameter/satellite-reference.h"
#include "object/uri.h"
#include "qt/ui/object-picker-button.h"
#include "selection.h"

namespace Inkscape {
namespace LivePathEffect {

OriginalSatelliteParam::OriginalSatelliteParam(const Glib::ustring &label, const Glib::ustring &tip,
                                               const Glib::ustring &key, Inkscape::UI::Widget::Registry *wr,
                                               Effect *effect)
    : SatelliteParam(label, tip, key, wr, effect)
{
}

QWidget* OriginalSatelliteParam::param_newWidget()
{
    if (!widget_is_visible) return nullptr;

    auto hbox = new QWidget();
    auto layout = new QHBoxLayout(hbox);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Pick item on canvas button
    auto pickButton = Linea::UI::create_object_picker_button([this](SPObject* obj) {
        if (obj && obj->getId()) {
            link(obj->getId());
        }
    }, "Pick item on canvas");
    layout->addWidget(pickButton);

    // Paste item to link button
    auto linkButton = new QPushButton();
    linkButton->setIcon(QIcon(":/icons/edit-paste"));
    linkButton->setFlat(true);
    linkButton->setToolTip(_("Link to item"));
    QObject::connect(linkButton, &QPushButton::clicked, [this]() { on_link_button_click(); });
    layout->addWidget(linkButton);

    // Select original button
    auto selectButton = new QPushButton();
    selectButton->setIcon(QIcon(":/icons/edit-select-original"));
    selectButton->setFlat(true);
    selectButton->setToolTip(_("Select original"));
    QObject::connect(selectButton, &QPushButton::clicked, [this]() { on_select_original_button_click(); });
    layout->addWidget(selectButton);

    return hbox;
}

void OriginalSatelliteParam::on_select_original_button_click()
{
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    auto original = cast<SPItem>(lperef->getObject());
    if (desktop == nullptr || original == nullptr) {
        return;
    }
    Inkscape::Selection *selection = desktop->getSelection();
    selection->clear();
    selection->set(original);
}

} /* namespace LivePathEffect */
} /* namespace Inkscape */
