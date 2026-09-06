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

#ifndef INKSCAPE_EVENT_LOG_H
#define INKSCAPE_EVENT_LOG_H

#include <QAbstractItemModel>
#include <QString>
#include <memory>
#include <vector>

#include "event.h"
#include "ui/operation-blocker.h"
#include "undo-stack-observer.h"

class SPDocument;

namespace Inkscape {

/**
 * A simple log for maintaining a history of committed, undone and redone events along with their
 * type. It implements the UndoStackObserver and should be registered with a
 * CompositeUndoStackObserver for each document. The event log is then notified on all commit, undo
 * and redo events and will store a representation of them in a Qt item model.
 *
 * Consecutive events of the same type are grouped with the first event as a parent and following
 * as its children.
 */
class EventLog
    : public QAbstractItemModel
    , public UndoStackObserver {
    Q_OBJECT

public:
    enum Column { IconColumn, ChildCountColumn, DescriptionColumn, ColumnCount };

    enum Role { EventRole = Qt::UserRole + 1, IconNameRole, ChildCountRole };

    explicit EventLog(SPDocument* document, QObject* parent = nullptr);
    ~EventLog() override;

    EventLog(const EventLog&) = delete;
    EventLog& operator=(const EventLog&) = delete;
    EventLog(EventLog&&) = delete;
    EventLog& operator=(EventLog&&) = delete;

    void notifyUndoEvent(Event* log) override;
    void notifyRedoEvent(Event* log) override;
    void notifyUndoCommitEvent(Event* log) override;
    void notifyUndoExpired(Event* log) override;
    void notifyClearUndoEvent() override;
    void notifyClearRedoEvent() override;

    QModelIndex currentIndex() const;
    Event* eventForIndex(const QModelIndex& index) const;
    QModelIndex indexForEvent(Event* event) const;

    void rememberFileSave() { _lastSaved = _currentEvent; }

    /// Update the sensitivity of undo and redo actions.
    void updateUndoVerbs();

    /// Seek the document to a given item in the undo history.
    void seekTo(const QModelIndex& target);

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

Q_SIGNALS:
    void currentEventChanged();

private:
    struct Node {
        Event* event = nullptr;
        QString iconName;
        QString description;
        int childCount = 0;
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
    };

    SPDocument* _document;
    Node _root;
    Node* _firstEvent;
    Node* _currentEvent;
    Node* _lastEvent;
    Node* _currentEventParent = nullptr;
    Node* _lastSaved;

    OperationBlocker _blocker;

    const Node* _getUndoEvent() const;
    const Node* _getRedoEvent() const;

    void _clearRedo();
    void _checkForVirginity();
    Node* nodeForIndex(const QModelIndex& index) const;
    QModelIndex _indexForNode(const Node* node, int column = 0) const;
    QModelIndex _indexForParent(const Node* node) const;
    static int _rowForNode(const Node* node);
    static QString _toQString(const Glib::ustring& value);
    static std::vector<int> _pathForNode(const Node* node);
    static Node* _lastChild(Node* node);
    void _moveToPrevious();
    void _moveToNext();
};

} // namespace Inkscape

#endif // INKSCAPE_EVENT_LOG_H
