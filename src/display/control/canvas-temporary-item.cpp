// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Provides a class that can contain active TemporaryItem's on a desktop
 * When the object is deleted, it also deletes the canvasitem it contains!
 * This object should be created/managed by a TemporaryItemList.
 * After its lifetime, it fires the timeout signal, afterwards *it deletes itself*.
 *
 * (part of code inspired by message-stack.cpp)
 *
 * Authors:
 *   Johan Engelen
 *
 * Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/main.h>

#include "canvas-temporary-item.h"
#include "canvas-item.h"
#include <QTimer>
#include <QObject>
#include <memory>

namespace Inkscape {
namespace Display {

TemporaryItem::TemporaryItem(CanvasItem *item, int lifetime_msecs)
    : canvasitem(std::move(item))
{
    // Zero lifetime means stay forever, so do not add timeout event.
    if (lifetime_msecs > 0) {
        _timer = std::make_unique<QTimer>();
        _timer->setSingleShot(true);
        _timer->setInterval(lifetime_msecs);
        QObject::connect(_timer.get(), &QTimer::timeout, [this] {
            signal_timeout.emit(this);
            delete this;
        });
        _timer->start();
    }
}

TemporaryItem::~TemporaryItem()
{
    if (_timer) {
        _timer->stop();
    }
}

} // namespace Display
} // namespace Inkscape
