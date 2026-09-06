// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PathOperations implementation.
 */

#include "path-operations.h"

#include <QPushButton>

#include "actions/action-registry.h"
#include "ui_path-operations.h"

namespace Linea::UI {

PathOperations::PathOperations(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PathOperations>()) {
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto sp = _ui->dummy->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    _ui->dummy->setSizePolicy(sp);

    // Auto-connect buttons via their "actionName" dynamic property
    auto& registry = ActionRegistry::get();
    for (auto button : findChildren<QPushButton*>()) {
        auto actionName = button->property("actionName").toString();
        if (actionName.isEmpty()) continue;
        connect(button, &QPushButton::clicked, this, [&registry, actionName] {
            registry.action(actionName.toUtf8().constData())->trigger();
        });
    }
}

PathOperations::~PathOperations() = default;

} // namespace Linea::UI
