// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Theme — helper functions for UI theming and styling.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QPixmapCache>
#include <QResource>
#include <QStyleHints>
#include <QWidget>

namespace Linea::UI {

/// @brief Mix two colors together
/// @param color1 First color
/// @param color2 Second color
/// @param ratio Ratio of color1 to color2 (0.0 = color1, 1.0 = color2)
/// @return Mixed color
QColor mixColors(const QColor& color1, const QColor& color2, double ratio = 0.5) {
    return QColor(
        color1.red() * (1 - ratio) + color2.red() * ratio,
        color1.green() * (1 - ratio) + color2.green() * ratio,
        color1.blue() * (1 - ratio) + color2.blue() * ratio,
        color1.alpha() * (1 - ratio) + color2.alpha() * ratio
    );
}

QColor fromColor(QColor color, float withAlpha) {
    color.setAlphaF(withAlpha);
    return color;
}

void setLightThemePalette(QPalette& palette) {
    const QColor light(0xf2f2f2);
    const QColor highlight(0xadd8fa);
    const QColor accent(0x45a7f3);
    const QColor button(0xe0e0e0);
    const auto mid = QColor::fromRgbF(0.0, 0.0, 0.0, 0.10);
    const auto text = QColor::fromRgbF(0.0, 0.0, 0.0, 0.85);
    const auto disabledText = fromColor(text, 0.5f);
    // fully opaque color for consistency; it covers some elements that
    // seem to render border in various shades of gray
    const auto midlight = QColor(0xe7e7e7);
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Window, light);
    palette.setColor(QPalette::AlternateBase, Qt::white);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::HighlightedText, text);
    // some borders; compare with mid and see if they can be unified
    palette.setColor(QPalette::Midlight, midlight);
    palette.setColor(QPalette::Button, button);
    // subtle border around list widgets and inputs
    palette.setColor(QPalette::Mid, mid);
    // dark - secondary text
    palette.setColor(QPalette::Dark, QColor::fromRgbF(0, 0, 0, 0.45f));
    palette.setColor(QPalette::Shadow, QColor::fromRgbF(0.0, 0.0, 0.0, 0.08));
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::Accent, accent);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, mixColors(highlight, light, 0.5));
    palette.setColor(QPalette::Disabled, QPalette::Accent, mixColors(accent, light, 0.5));
    palette.setColor(QPalette::Disabled, QPalette::Mid, mid);
    palette.setColor(QPalette::Disabled, QPalette::Midlight, midlight);
}

void setDarkThemePalette(QPalette& palette) {
    const QColor dark(0x222625);
    auto base = mixColors(dark, QColor(Qt::black), 0.15);
    const QColor highlight(0x2f9bf0);
    const QColor accent(0x2f9bf0);
    const QColor button(0x323635);
    const auto mid = QColor::fromRgbF(1.0, 1.0, 1.0, 0.10);
    const auto text = QColor::fromRgbF(1.0, 1.0, 1.0, 0.85);
    const auto disabledText = fromColor(text, 0.5f);
    const auto midlight = QColor(0x292d2c);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::Window, dark);
    palette.setColor(QPalette::AlternateBase, base);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::HighlightedText, text);
    palette.setColor(QPalette::Midlight, midlight);
    palette.setColor(QPalette::Mid, mid);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::Dark, QColor::fromRgbF(1.0, 1.0, 1.0, 0.55));
    // shadow: not used in styles.qss
    palette.setColor(QPalette::Shadow, QColor::fromRgbF(1.0, 1.0, 1.0, 0.08));
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::Accent, accent);
    palette.setColor(QPalette::ToolTipBase, QColor::fromRgbF(1, 1, 1, 0.50));
    palette.setColor(QPalette::ToolTipText, QColor::fromRgbF(0, 0, 0, 1.0));
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, mixColors(highlight, dark, 0.5));
    palette.setColor(QPalette::Disabled, QPalette::Accent, mixColors(accent, dark, 0.5));
    palette.setColor(QPalette::Disabled, QPalette::Mid, mid);
    palette.setColor(QPalette::Disabled, QPalette::Midlight, midlight);
}

QColor getBackgroundColor() {
    return QApplication::palette().color(QPalette::Window);
}

QColor getForegroundColor() {
    return QApplication::palette().color(QPalette::WindowText);
}

QColor getAccentColor() {
    return QApplication::palette().color(QPalette::Highlight);
}

bool isDarkTheme() {
    QPalette palette = QApplication::palette();
    QColor windowColor = palette.color(QPalette::Window);
    QColor textColor = palette.color(QPalette::WindowText);

    // Calculate luminance
    double windowLuminance = (0.299 * windowColor.red() + 0.587 * windowColor.green() + 0.114 * windowColor.blue()) / 255.0;
    double textLuminance = (0.299 * textColor.red() + 0.587 * textColor.green() + 0.114 * textColor.blue()) / 255.0;

    // Dark theme if background is darker than text
    return windowLuminance < textLuminance;
}

void setIconTheme(bool dark) {
    static QByteArray lightRccData;
    static QByteArray darkRccData;

    // Unregister current icon resource if any
    if (!lightRccData.isEmpty()) {
        QResource::unregisterResource(reinterpret_cast<const uchar*>(lightRccData.constData()), "/icons");
        lightRccData.clear();
    }
    if (!darkRccData.isEmpty()) {
        QResource::unregisterResource(reinterpret_cast<const uchar*>(darkRccData.constData()), "/icons");
        darkRccData.clear();
    }

    // Read the embedded .rcc data for the chosen icon set
    QString rccResource = dark ? ":/icons-dark.rcc" : ":/icons-light.rcc";
    QByteArray& rccData = dark ? darkRccData : lightRccData;

    QFile file(rccResource);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    rccData = file.readAll();
    file.close();

    if (rccData.isEmpty()) {
        return;
    }

    // Register the raw .rcc data under /icons
    if (QResource::registerResource(reinterpret_cast<const uchar*>(rccData.constData()), "/icons")) {
        // Force widget repaint to refresh icons
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            widget->update();
        }
    }
}

void setApplicationTheme(bool dark, bool followSystem) {
    QApplication::styleHints()->setColorScheme(
        followSystem ? Qt::ColorScheme::Unknown
                     : (dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light));

    auto pal = QApplication::palette();
    if (dark) {
        setDarkThemePalette(pal);
    }
    else {
        setLightThemePalette(pal);
    }
    QApplication::setPalette(pal);

    setIconTheme(dark);
    QPixmapCache::clear();

    // Re-apply the stylesheet so palette() references resolve against the new palette
    QString qss = qApp->styleSheet();
    qApp->setStyleSheet(QString{});
    qApp->setStyleSheet(qss);
}

} // namespace Linea::UI
