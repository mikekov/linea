// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2012 <j.b.c.engelen@alumnus.utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/originalpath.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <glibmm/i18n.h>

#include "desktop.h"
#include "inkscape.h"
#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "object/sp-shape.h"
#include "qt/ui/object-picker-button.h"
#include "selection.h"

namespace Inkscape {
namespace LivePathEffect {

OriginalPathParam::OriginalPathParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect)
    : PathParam(label, tip, key, wr, effect, "")
{
    oncanvas_editable = false;
    _from_original_d = false;
}

QWidget*
OriginalPathParam::param_newWidget()
{
    if (!widget_is_visible) return nullptr;

    auto widget = new QWidget();
    auto layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto pickBtn = Linea::UI::create_object_picker_button([this](SPObject* obj) {
        if (obj && obj->getId()) {
            linkitem(obj->getId());
        }
    }, "Pick path on canvas");
    layout->addWidget(pickBtn);

    auto linkBtn = new QPushButton();
    linkBtn->setIcon(QIcon(":/icons/edit-clone"));
    linkBtn->setToolTip(QObject::tr("Link to path in clipboard"));
    QObject::connect(linkBtn, &QPushButton::clicked, [this] { on_link_button_click(); });
    layout->addWidget(linkBtn);

    auto selectBtn = new QPushButton();
    selectBtn->setIcon(QIcon(":/icons/edit-select-original"));
    selectBtn->setToolTip(QObject::tr("Select original"));
    QObject::connect(selectBtn, &QPushButton::clicked, [this] { on_select_original_button_click(); });
    layout->addWidget(selectBtn);

    return widget;
}

void
OriginalPathParam::on_select_original_button_click()
{
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    SPItem *original = ref.getObject();
    if (desktop == nullptr || original == nullptr) {
        return;
    }
    Inkscape::Selection *selection = desktop->getSelection();
    selection->clear();
    selection->set(original);
    param_effect->getLPEObj()->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

} /* namespace LivePathEffect */
} /* namespace Inkscape */
