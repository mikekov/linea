// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Gustav Broberg <broberg@kth.se>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (c) 2014 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "event-log.h"

#include <algorithm>
#include <cassert>
#include <glibmm/i18n.h>

#include "actions/actions-undo-document.h"
#include "document-undo.h"
#include "document.h"

namespace Inkscape {

EventLog::EventLog(SPDocument* document, QObject* parent)
    : QAbstractItemModel{parent}
    , _document{document}
    , _firstEvent{nullptr}
    , _currentEvent{nullptr}
    , _lastEvent{nullptr}
    , _lastSaved{nullptr} {
    _root.children.push_back(std::make_unique<Node>());
    _firstEvent = _root.children.front().get();
    _currentEvent = _lastEvent = _lastSaved = _firstEvent;
    _firstEvent->iconName = QStringLiteral("document-new");
    _firstEvent->description = _toQString(_("[No more changes]"));
}

EventLog::~EventLog() = default;

QString EventLog::_toQString(const Glib::ustring& value) {
    return QString::fromUtf8(value.c_str());
}

int EventLog::_rowForNode(const Node* node) {
    if (!node || !node->parent) {
        return -1;
    }

    const auto& children = node->parent->children;
    auto iter =
        std::find_if(children.begin(), children.end(), [node](const auto& child) { return child.get() == node; });
    return iter == children.end() ? -1 : static_cast<int>(std::distance(children.begin(), iter));
}

QModelIndex EventLog::_indexForNode(const Node* node, int column) const {
    if (!node || node == &_root) {
        return {};
    }
    auto row = _rowForNode(node);
    return row < 0 ? QModelIndex{} : createIndex(row, column, const_cast<Node*>(node));
}

QModelIndex EventLog::_indexForParent(const Node* node) const {
    return node && node->parent && node->parent != &_root ? _indexForNode(node->parent) : QModelIndex{};
}

EventLog::Node* EventLog::nodeForIndex(const QModelIndex& index) const {
    if (!index.isValid() || index.model() != this) {
        return nullptr;
    }
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex EventLog::currentIndex() const {
    return _indexForNode(_currentEvent);
}

Event* EventLog::eventForIndex(const QModelIndex& index) const {
    auto node = nodeForIndex(index);
    return node ? node->event : nullptr;
}

QModelIndex EventLog::indexForEvent(Event* event) const {
    if (!event) {
        return {};
    }

    std::vector<const Node*> pending;
    for (const auto& child : _root.children) {
        pending.push_back(child.get());
    }
    while (!pending.empty()) {
        auto node = pending.back();
        pending.pop_back();
        if (node->event == event) {
            return _indexForNode(node);
        }
        for (const auto& child : node->children) {
            pending.push_back(child.get());
        }
    }
    return {};
}

QModelIndex EventLog::index(int row, int column, const QModelIndex& parent) const {
    if (column < 0 || column >= ColumnCount || row < 0) {
        return {};
    }

    auto parentNode = parent.isValid() ? nodeForIndex(parent) : const_cast<Node*>(&_root);
    if (!parentNode || row >= static_cast<int>(parentNode->children.size())) {
        return {};
    }
    return _indexForNode(parentNode->children[row].get(), column);
}

QModelIndex EventLog::parent(const QModelIndex& child) const {
    auto node = nodeForIndex(child);
    return node ? _indexForParent(node) : QModelIndex{};
}

int EventLog::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() && parent.column() != 0) {
        return 0;
    }
    auto node = parent.isValid() ? nodeForIndex(parent) : const_cast<Node*>(&_root);
    return node ? static_cast<int>(node->children.size()) : 0;
}

int EventLog::columnCount(const QModelIndex&) const {
    return ColumnCount;
}

QVariant EventLog::data(const QModelIndex& index, int role) const {
    auto node = nodeForIndex(index);
    if (!node) {
        return {};
    }

    if (role == EventRole) {
        return QVariant::fromValue(reinterpret_cast<quintptr>(node->event));
    }
    if (role == IconNameRole) {
        return node->iconName;
    }
    if (role == ChildCountRole) {
        return node->childCount;
    }
    if (role == Qt::DecorationRole && index.column() == IconColumn) {
        return node->iconName;
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
        case IconColumn:
            return node->iconName;
        case ChildCountColumn:
            return node->childCount;
        case DescriptionColumn:
            return node->description;
        default:
            return {};
    }
}

QVariant EventLog::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= ColumnCount) {
        return {};
    }
    switch (section) {
        case IconColumn:
            return QStringLiteral("Icon");
        case ChildCountColumn:
            return QStringLiteral("Children");
        case DescriptionColumn:
            return QStringLiteral("Description");
        default:
            return {};
    }
}

Qt::ItemFlags EventLog::flags(const QModelIndex& index) const {
    return index.isValid() ? QAbstractItemModel::flags(index) : Qt::NoItemFlags;
}

void EventLog::notifyUndoEvent(Event* log) {
    if (_blocker.pending()) {
        return;
    }

    auto undoEvent = _getUndoEvent();
    if (!undoEvent || undoEvent->event != log) {
        return;
    }

    _moveToPrevious();
    _checkForVirginity();
    updateUndoVerbs();
    Q_EMIT currentEventChanged();
}

void EventLog::notifyRedoEvent(Event* log) {
    if (_blocker.pending()) {
        return;
    }

    auto redoEvent = _getRedoEvent();
    if (!redoEvent || redoEvent->event != log) {
        return;
    }

    _moveToNext();
    _checkForVirginity();
    updateUndoVerbs();
    Q_EMIT currentEventChanged();
}

void EventLog::notifyUndoCommitEvent(Event* log) {
    if (!log) {
        return;
    }

    auto iconName = _toQString(log->icon_name);
    Node* parent = nullptr;
    int row = 0;

    if (_currentEvent->iconName == iconName) {
        if (!_currentEventParent) {
            _currentEventParent = _currentEvent;
        }
        parent = _currentEventParent;
        row = static_cast<int>(parent->children.size());
    } else {
        parent = &_root;
        row = static_cast<int>(parent->children.size());
        _currentEventParent = nullptr;
    }

    auto parentIndex = parent == &_root ? QModelIndex{} : _indexForNode(parent);
    beginInsertRows(parentIndex, row, row);
    auto node = std::make_unique<Node>();
    node->event = log;
    node->iconName = std::move(iconName);
    node->description = _toQString(log->description);
    node->childCount = parent == &_root ? 1 : 0;
    node->parent = parent;
    auto inserted = node.get();
    parent->children.push_back(std::move(node));
    endInsertRows();

    if (parent != &_root) {
        parent->childCount = static_cast<int>(parent->children.size()) + 1;
        auto parentModelIndex = _indexForNode(parent);
        Q_EMIT dataChanged(parentModelIndex, parentModelIndex, {ChildCountRole, Qt::DisplayRole});
    }

    _currentEvent = _lastEvent = inserted;
    _checkForVirginity();
    updateUndoVerbs();
    Q_EMIT currentEventChanged();
}

void EventLog::notifyUndoExpired(Event* log) {
    if (_root.children.size() == 1) {
        return;
    }

    auto iter = _root.children.begin() + 1;
    auto node = iter->get();
    if (!node || node->event != log) {
        return;
    }

    if (!node->children.empty()) {
        auto parentIndex = _indexForNode(node);
        beginRemoveRows(parentIndex, 0, 0);
        auto child = std::move(node->children.front());
        node->event = child->event;
        node->description = child->description;
        node->iconName = child->iconName;
        node->children.erase(node->children.begin());
        node->childCount = static_cast<int>(node->children.size());
        endRemoveRows();

        Q_EMIT dataChanged(parentIndex, parentIndex);
    } else {
        beginRemoveRows({}, 1, 1);
        _root.children.erase(iter);
        endRemoveRows();
    }

    _firstEvent->description =
        _firstEvent->childCount == 0 ? _toQString(_("[Changes forgotten]")) : _firstEvent->description;
    ++_firstEvent->childCount;
    auto firstIndex = _indexForNode(_firstEvent);
    Q_EMIT dataChanged(firstIndex, firstIndex, {ChildCountRole, Qt::DisplayRole});
}

void EventLog::notifyClearUndoEvent() {
    updateUndoVerbs();
}

void EventLog::notifyClearRedoEvent() {
    _clearRedo();
    updateUndoVerbs();
}

void EventLog::updateUndoVerbs() {
    if (_document) {
        enable_undo_actions(_document, _getUndoEvent() != nullptr, _getRedoEvent() != nullptr);
    }
}

std::vector<int> EventLog::_pathForNode(const Node* node) {
    std::vector<int> path;
    while (node && node->parent) {
        path.push_back(_rowForNode(node));
        node = node->parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void EventLog::seekTo(const QModelIndex& target) {
    if (_blocker.pending()) {
        return;
    }
    auto targetNode = nodeForIndex(target);
    if (!targetNode || targetNode == &_root) {
        return;
    }

    auto guard = _blocker.block();
    auto targetPath = _pathForNode(targetNode);
    auto currentPath = _pathForNode(_currentEvent);

    if (targetPath < currentPath) {
        while (_currentEvent != targetNode) {
            DocumentUndo::undo(_document);
            _moveToPrevious();
        }
    } else {
        while (_currentEvent != targetNode) {
            DocumentUndo::redo(_document);
            _moveToNext();
        }
    }

    _checkForVirginity();
    updateUndoVerbs();
    Q_EMIT currentEventChanged();
}

const EventLog::Node* EventLog::_getUndoEvent() const {
    return _currentEvent == _firstEvent ? nullptr : _currentEvent;
}

const EventLog::Node* EventLog::_getRedoEvent() const {
    if (_currentEvent == _lastEvent) {
        return nullptr;
    }
    if (!_currentEvent->children.empty()) {
        return _currentEvent->children.front().get();
    }

    auto node = _currentEvent;
    while (node->parent) {
        auto row = _rowForNode(node);
        if (row + 1 < static_cast<int>(node->parent->children.size())) {
            return node->parent->children[row + 1].get();
        }
        node = node->parent;
    }
    return nullptr;
}

EventLog::Node* EventLog::_lastChild(Node* node) {
    if (!node || node->children.empty()) {
        return node;
    }
    return node->children.back().get();
}

void EventLog::_moveToPrevious() {
    if (_currentEvent->parent && _currentEvent == _currentEvent->parent->children.front().get()) {
        _currentEvent = _currentEvent->parent;
        _currentEventParent = nullptr;
        return;
    }

    auto row = _rowForNode(_currentEvent);
    if (row <= 0) {
        return;
    }
    _currentEvent = _lastChild(_currentEvent->parent->children[row - 1].get());
    _currentEventParent = _currentEvent->children.empty() ? nullptr : _currentEvent;
}

void EventLog::_moveToNext() {
    if (!_currentEvent->children.empty()) {
        _currentEventParent = _currentEvent;
        _currentEvent = _currentEvent->children.front().get();
        return;
    }

    auto node = _currentEvent;
    while (node->parent) {
        auto row = _rowForNode(node);
        if (row + 1 < static_cast<int>(node->parent->children.size())) {
            _currentEvent = node->parent->children[row + 1].get();
            _currentEventParent = nullptr;
            return;
        }
        node = node->parent;
    }
}

void EventLog::_clearRedo() {
    auto guard = _blocker.block();
    if (_lastEvent == _currentEvent) {
        return;
    }

    if (!_currentEvent->children.empty()) {
        auto parentIndex = _indexForNode(_currentEvent);
        auto last = static_cast<int>(_currentEvent->children.size()) - 1;
        beginRemoveRows(parentIndex, 0, last);
        _currentEvent->children.clear();
        endRemoveRows();
    }

    auto node = _currentEvent;
    while (node->parent) {
        auto parent = node->parent;
        auto row = _rowForNode(node);
        auto first = row + 1;
        if (first < static_cast<int>(parent->children.size())) {
            auto parentIndex = parent == &_root ? QModelIndex{} : _indexForNode(parent);
            auto last = static_cast<int>(parent->children.size()) - 1;
            beginRemoveRows(parentIndex, first, last);
            parent->children.erase(parent->children.begin() + first, parent->children.end());
            endRemoveRows();
        }
        node = parent;
    }

    _lastEvent = _currentEvent;
    _currentEventParent = _currentEvent->children.empty() ? nullptr : _currentEvent;
}

void EventLog::_checkForVirginity() {
    if (_document && _currentEvent == _lastSaved) {
        _document->setModifiedSinceSave(false);
    }
}

} // namespace Inkscape
