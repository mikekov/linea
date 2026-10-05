// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontBrowserOptions — font preview options popup content.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 */

#include "font-browser-options.h"

#include <QMenu>
#include <QSignalBlocker>
#include <QSlider>

#include "number-edit.h"
#include "ui_font-browser-options.h"

namespace Linea::UI {

FontBrowserOptions::FontBrowserOptions(QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::FontBrowserOptions>()) {
    ui->setupUi(this);

    connect(ui->sampleEdit, &QLineEdit::textChanged, this, &FontBrowserOptions::sampleTextChanged);

    // presets menu: "Font name" restores the default (empty sample)
    auto presets = new QMenu(this);
    presets->addAction(tr("Font name"), this, [this] { ui->sampleEdit->clear(); });
    presets->addSeparator();
    const QStringList samples = {
        tr("AbcdEfgh1234"),
        tr("1234567890"),
        tr("abcdefghijklmnopqrstuvwxyz"),
        tr("ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
        tr("The quick brown fox jumps over the lazy dog."),
        tr("Yélløw ťüřtle fröm Áłphårettä íś čōmińġ fôr ďïññęr tòđây.")
    };
    for (auto& text : samples) {
        presets->addAction(text, this, [this, text] { ui->sampleEdit->setText(text); });
    }
    ui->samplePresetsButton->setMenu(presets);

    connect(ui->showFontNameCheck, &QCheckBox::toggled, this, &FontBrowserOptions::showFontNameChanged);

    // slider snaps to steps of 10; slider and edit stay in sync
    connect(ui->sizeSlider, &QSlider::valueChanged, this, [this](int value) {
        int stepped = (value + 5) / 10 * 10;
        if (stepped != value) {
            QSignalBlocker blocker(ui->sizeSlider);
            ui->sizeSlider->setValue(stepped);
        }
        QSignalBlocker blocker(ui->sizeEdit);
        ui->sizeEdit->setValue(stepped);
        Q_EMIT previewPercentChanged(stepped);
    });
    connect(ui->sizeEdit, &NumberEdit::valueChanged, this, [this](double value) {
        int percent = static_cast<int>(value);
        QSignalBlocker blocker(ui->sizeSlider);
        ui->sizeSlider->setValue(percent);
        Q_EMIT previewPercentChanged(percent);
    });
}

FontBrowserOptions::~FontBrowserOptions() = default;

void FontBrowserOptions::setSampleText(const QString& text) {
    QSignalBlocker blocker(ui->sampleEdit);
    ui->sampleEdit->setText(text);
}

QString FontBrowserOptions::sampleText() const {
    return ui->sampleEdit->text();
}

void FontBrowserOptions::setShowFontName(bool show) {
    QSignalBlocker blocker(ui->showFontNameCheck);
    ui->showFontNameCheck->setChecked(show);
}

bool FontBrowserOptions::showFontName() const {
    return ui->showFontNameCheck->isChecked();
}

void FontBrowserOptions::setPreviewPercent(int percent) {
    QSignalBlocker blockerSlider(ui->sizeSlider);
    QSignalBlocker blockerEdit(ui->sizeEdit);
    ui->sizeSlider->setValue(percent);
    ui->sizeEdit->setValue(percent);
}

int FontBrowserOptions::previewPercent() const {
    return ui->sizeSlider->value();
}

} // namespace Linea::UI
