// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ActionAccel class implementation
 *
 * Authors:
 *   Rafael Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2022 the Authors.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "util/action-accel.h"

#include <utility>

#include <QString>

#include "ui/shortcut-manager.h"
#include "ui/widget/events/canvas-event.h"

namespace Inkscape::Util {

ActionAccel::ActionAccel(std::string action_name)
    : _action{std::move(action_name)}
{
    _query();
    _prefs_changed = ShortcutManager::instance().shortcutChanged.connect(
        [this](const QString& action_id, const QKeySequence&) { _onShortcutsModified(action_id, {}); });
}

void ActionAccel::_onShortcutsModified(const QString& action_id, const QKeySequence&) {
    // ShortcutManager emits for any action; filter to our own.
    if (action_id.isEmpty() || action_id.toStdString() == _action) {
        if (_query()) {
            _we_changed.emit();
        }
    }
}

bool ActionAccel::_query()
{
    const auto seqs = ShortcutManager::instance().shortcuts(QString::fromStdString(_action));
    std::set<QKeySequence> new_keys{seqs.begin(), seqs.end()};
    if (new_keys == _accels) {
        return false;
    }

    _accels = std::move(new_keys);
    return true;
}

bool ActionAccel::isTriggeredBy(KeyEvent const &key) const
{
    // Build a QKeySequence from the Qt key + modifiers carried by the event.
    auto const seq = QKeySequence(key.qtKey | key.qtModifiers);
    return _accels.find(seq) != _accels.end();
}

std::vector<std::string> ActionAccel::getShortcutText() const {
    std::vector<std::string> labels;
    for (const auto& accel : _accels) {
        labels.emplace_back(accel.toString(QKeySequence::NativeText).toStdString());
    }
    return labels;
}

} // namespace Inkscape::Util
