// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * createColorClipboardMenu — Copy/Paste color menu bound to a ColorHolder.
 */

#include "color-clipboard-menu.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>

#include "color-holder.h"
#include "colors/color.h"
#include "colors/spaces/base.h"

namespace Linea::UI {

using namespace Inkscape::Colors;

namespace {

std::optional<Color> clipboardColor() {
    return Color::parse(QGuiApplication::clipboard()->text().trimmed().toStdString());
}

} // namespace

QMenu* createColorClipboardMenu(std::shared_ptr<ColorHolder> colors, std::function<Space::Type()> display_space,
                                QWidget* parent) {
    auto menu = new QMenu(parent);

    auto copy = menu->addAction(QIcon(":/icons/edit-copy"), QObject::tr("Copy"));
    QObject::connect(copy, &QAction::triggered, menu, [colors, display_space] {
        if (!colors || colors->isEmpty()) return;
        // toString() emits the color's own CSS function (oklch(), lab(), hex,
        // ...) — all of which Color::parse accepts — so the value round-trips
        // without an sRGB gamut clamp. A display space override (e.g. the
        // picker's) serializes in that space instead.
        auto color = colors->getOrDefault();
        if (display_space) {
            if (auto c = color.converted(display_space())) {
                color = *c;
            }
        }
        QGuiApplication::clipboard()->setText(QString::fromStdString(color.toString()));
    });

    auto copy_hex = menu->addAction(QIcon(":/icons/edit-copy"), QObject::tr("Copy as Hex"));
    QObject::connect(copy_hex, &QAction::triggered, menu, [colors] {
        if (!colors || colors->isEmpty()) return;
        if (auto color = colors->getOrDefault().converted(Space::Type::RGB)) {
            QGuiApplication::clipboard()->setText(QString::fromStdString(color->toString(false)));
        }
    });

    auto paste = menu->addAction(QIcon(":/icons/edit-paste"), QObject::tr("Paste"));
    QObject::connect(paste, &QAction::triggered, menu, [colors] {
        if (colors) {
            if (auto color = clipboardColor()) {
                colors->set(*color);
            }
        }
    });

    QObject::connect(menu, &QMenu::aboutToShow, menu, [colors, copy, copy_hex, paste] {
        const bool can_copy = colors && !colors->isEmpty();
        copy->setEnabled(can_copy);
        copy_hex->setEnabled(can_copy);
        paste->setEnabled(colors && clipboardColor().has_value());
    });

    return menu;
}

QMenu* createColorClipboardMenu(std::shared_ptr<ColorHolder> colors, QWidget* parent) {
    return createColorClipboardMenu(colors, nullptr, parent);
}

} // namespace Linea::UI
