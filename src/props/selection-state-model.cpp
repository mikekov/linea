// SPDX-License-Identifier: GPL-2.0-or-later

#include "props/selection-state-model.h"

#include <QObject>
#include <QTimer>

#include "desktop.h"
#include "object/sp-object.h"
#include "preferences.h"
#include "selection.h"
#include "ui/tools/text-tool.h"

namespace Linea::Props {

SelectionStateModel::SelectionStateModel(SPDesktop* desktop)
    : _selection(desktop->getSelection())
    , _desktop(desktop)
    , _timerGuard(std::make_unique<QObject>()) {

    _changedConn = _selection->connectChanged([this](auto) { scheduleRebuild(0); });
    _modifiedConn = _selection->connectModified([this](auto, unsigned flags) { scheduleRebuild(flags); });

    // Tool changes affect the text-span scope: when the text tool is
    // active, the scope lambda returns span items; when it's not, the
    // scope returns empty and the model falls back to the selection.
    // The rebuild re-evaluates the scope and updates has_text_subselection.
    _toolChangedConn = _desktop->connectEventContextChanged([this](auto, auto tool) {
        tools_enum next = TOOLS_INVALID;
        if (tool) {
            if (auto data = getToolData(tool->get_name())) {
                next = static_cast<tools_enum>(data->tool);
            }
        }
        if (next != _activeTool) {
            _activeTool = next;
            scheduleRebuild(0);
        }
    });

    _cursor_moved = desktop->connect_text_cursor_moved([this](Inkscape::UI::Tools::TextTool* tool) {
        if (tool) {
            // cursor move -> subselection change, we need to refresh text style properties
            scheduleRebuild(0);
        }
    });
}

SelectionStateModel::~SelectionStateModel() = default;

void SelectionStateModel::setTextScope(ItemScope scope) {
    _textScope = std::move(scope);
    scheduleRebuild(0);
}

void SelectionStateModel::setTargetScope(TargetScope scope) {
    _targetScope = std::move(scope);
}

void SelectionStateModel::scheduleRebuild(unsigned flags) {
    _pendingFlags |= flags;
    if (_rebuildScheduled) return;

    _rebuildScheduled = true;
    QTimer::singleShot(0, _timerGuard.get(), [this] {
        _rebuildScheduled = false;
        rebuildNow();
    });
}

void SelectionStateModel::rebuildNow() {
    SelectionState next;

    next.element.count.has_text_subselection = forEachLeafItem([&next](SPObject* item) { merge_item(next, item); });
    next.element.count.activeTool = _activeTool;
    next.element.count.text_tool_active = (_activeTool == TOOLS_TEXT);

    // Selection bounding box in px. The bbox type follows the user preference:
    // visual bounds include stroke thickness; geometric bounds do not.
    // Storing the preference-correct box means the geometry dirty bit fires
    // on stroke changes only when the user cares about visual bounds.
    if (_selection && !_selection->isEmpty()) {
        bool use_visual = Preferences::get()->getInt("/tools/bounding_box") == 0;
        next.bbox = _selection->bounds(use_visual ? SPItem::VISUAL_BBOX : SPItem::GEOMETRIC_BBOX);
    }

    auto delta = diff(_current, next);
    unsigned origin = _pendingFlags & SP_OBJECT_USER_TAGS_ALL;
    _pendingFlags = 0;
    if (delta.none()) return;

    next.revision = _current.revision + 1;
    _current = std::move(next);
    _signal_changed.emit(_current, delta, origin);
}

bool SelectionStateModel::forEachLeafItem(const std::function<void (SPObject*)>& fn) {
    if (!_selection) return false;

    // Text-span scope replaces the selected text objects entirely: when the
    // text tool selects spans, the snapshot reflects the span style.
    if (_textScope) {
        if (auto spans = _textScope(); !spans.empty()) {
            for (auto item : spans) {
                fn(item);
            }
            return true;
        }
    }

    for (auto obj : _selection->objects()) {
        fn(obj);
    }

    return false;
}

} // namespace Linea::Props
