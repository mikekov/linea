// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SwatchEditor — edits document color swatches stored as CSS custom properties.
 */

#include "swatch-editor.h"

#include <QIcon>
#include <QListWidget>
#include <QObject>
#include <QVBoxLayout>
#include <algorithm>
#include <glibmm/i18n.h>

#include "color-holder.h"
#include "color-picker-panel.h"
#include "color-preview.h"
#include "color-plate.h"
#include "colors/color.h"
#include "colors/manager.h"
#include "document-undo.h"
#include "document.h"
#include "preferences.h"
#include "style-helpers.h"
#include "ui_swatch-editor.h"
#include "util-string/context-string.h"

namespace Linea::UI {

namespace {

constexpr int swatchIconSize = 20;
constexpr char pickerTypePref[] = "/dialogs/swatchEditor/colorPickerType";

Inkscape::Colors::Space::Type pickerSpace() {
    auto name = Inkscape::Preferences::get()->getString(pickerTypePref, "HSL");
    auto space = Inkscape::Colors::Manager::get().find(name.raw());
    return space ? space->getType() : Inkscape::Colors::Space::Type::HSL;
}

QIcon swatchIcon(const Inkscape::Colors::Color& color) {
    ColorPreview preview(nullptr, color.toRGBA());
    preview.setStyle(ColorPreview::Outlined);
    preview.resize(swatchIconSize, swatchIconSize);
    // A parentless widget renders as an opaque top-level window; translucent
    // keeps the rounded rect's corners clear so the row's selection color shows through.
    preview.setAttribute(Qt::WA_TranslucentBackground);
    return QIcon(preview.grab());
}

const Inkscape::Colors::Color gray(Inkscape::Colors::Space::Type::RGB, {0.5, 0.5, 0.5, 1.0});
} // namespace

SwatchEditor::SwatchEditor(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::SwatchEditor>()) {
    _ui->setupUi(this);

    _colors = std::make_shared<ColorHolder>();
    // Seed the holder so the picker's sliders/plates initialize with a real
    // color even before any swatch is selected.
    _colors->set(gray);
    auto space = pickerSpace();
    _picker = ColorPickerPanel::create(space, ColorPlate::plateTypeFor(space), _colors, this);
    _picker->colorSpaceChanged().connect([space](Inkscape::Colors::Space::Type type) {
        if (auto space = Inkscape::Colors::Manager::get().find(type)) {
            Inkscape::Preferences::get()->setString(pickerTypePref, space->getName());
        }
    });
    auto pickerLayout = new QVBoxLayout(_ui->pickerPlaceholder);
    pickerLayout->setContentsMargins(0, 0, 0, 0);
    pickerLayout->addWidget(_picker.get());

    _sheet = std::make_unique<Inkscape::Css::DocumentStyleSheet>([this] { onStyleContentChanged(); });

    connect(_ui->swatchList, &QListWidget::currentRowChanged, this, &SwatchEditor::onSwatchSelected);
    connect(_ui->swatchList, &QListWidget::itemChanged, this, &SwatchEditor::onItemRenamed);
    connect(_ui->addButton, &QPushButton::clicked, this, &SwatchEditor::onAdd);
    connect(_ui->deleteButton, &QPushButton::clicked, this, &SwatchEditor::onDelete);
    _colorConn = _colors->signal_changed.connect([this] { onColorChanged(); });
}

SwatchEditor::~SwatchEditor() = default;

void SwatchEditor::setDocument(SPDocument* document) {
    if (_document == document) return;

    _document = document;
    _sheet->setDocument(document);
    // onStyleContentChanged skips reloads during pending updates; make
    // sure the list reflects the (possibly empty) document regardless.
    reload();
}

void SwatchEditor::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
    _picker->setDesktop(desktop);
}

void SwatchEditor::onStyleContentChanged() {
    if (_update.pending()) return;

    reload();
}

void SwatchEditor::reload() {
    // Preserve the selection across the rebuild by swatch name.
    const std::string selectedName =
        _selected >= 0 && _selected < static_cast<int>(_swatches.size()) ? _swatches[_selected].cssName() : std::string();

    _swatches.clear();
    for (auto& prop : _sheet->customProperties()) {
        auto color = Inkscape::Colors::Color::parse(prop.value);
        if (!color) {
            // Not a color — preserved in the stylesheet but not a swatch.
            continue;
        }
        _swatches.push_back({prop, std::move(*color)});
    }

    auto scoped = _update.block();
    QSignalBlocker listBlocker(_ui->swatchList);
    _ui->swatchList->clear();
    for (const auto& swatch : _swatches) {
        auto item = new QListWidgetItem(swatchIcon(swatch.color()), QString::fromStdString(swatch.displayName()));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setToolTip(QString::fromStdString(swatch.displayName() + ": " + swatch.color().toString()));
        _ui->swatchList->addItem(item);
    }

    _selected = -1;
    for (int i = 0; i < static_cast<int>(_swatches.size()); ++i) {
        if (_swatches[i].cssName() == selectedName) {
            _selected = i;
            break;
        }
    }
    _ui->swatchList->setCurrentRow(_selected);
    _ui->deleteButton->setEnabled(_selected >= 0);
    pushSelectionToPicker();
}

void SwatchEditor::pushSelectionToPicker() {
    auto scoped = _update.block();
    const bool has = _selected >= 0 && _selected < static_cast<int>(_swatches.size());
    _colors->set(has ? _swatches[_selected].color() : gray);
    _picker->setEnabled(has);
}

void SwatchEditor::onSwatchSelected(int row) {
    _selected = row >= 0 && row < static_cast<int>(_swatches.size()) ? row : -1;
    _ui->deleteButton->setEnabled(_selected >= 0);
    pushSelectionToPicker();
}

void SwatchEditor::onItemRenamed(QListWidgetItem* item) {
    if (_update.pending() || !_document || !item) return;

    const int row = _ui->swatchList->row(item);
    if (row < 0 || row >= static_cast<int>(_swatches.size())) return;

    auto& swatch = _swatches[row];

    auto revert = [&] {
        const QSignalBlocker blocker(_ui->swatchList);
        item->setText(QString::fromStdString(swatch.displayName()));
    };

    const std::string oldName = swatch.cssName();
    const std::string newName = swatch.tryRename(item->text().toStdString());
    if (newName.empty() || newName == oldName) {
        // Invalid input, or normalization made no change — restore the display.
        revert();
        return;
    }

    auto css = _sheet->text();
    if (Inkscape::Css::rename_root_custom_property(css, oldName, newName)) {
        // Update the name before writeCss — reload() preserves the selection
        // by name, and the rebuilt list only contains the new name.
        swatch.setName(newName);
        writeCss(std::move(css));
    } else {
        // Rejected by the stylesheet (e.g. duplicates an existing name).
        revert();
    }
}

void SwatchEditor::onColorChanged() {
    if (_update.pending() || !_document || _selected < 0 || _selected >= static_cast<int>(_swatches.size())) {
        return;
    }
    const auto& color = _colors->get();
    if (!color) return;

    auto css = _sheet->text();
    if (Inkscape::Css::set_root_custom_property(css, _swatches[_selected].cssName(), color->toString())) {
        _swatches[_selected].setColor(*color);
        writeCss(std::move(css));
    }
}

void SwatchEditor::onAdd() {
    if (!_document) return;

    // Find the first unused --swatch-N name.
    std::string name;
    for (int i = 1;; ++i) {
        name = "--swatch-" + std::to_string(i);
        if (std::none_of(_swatches.begin(), _swatches.end(), [&name](const Swatch& s) { return s.cssName() == name; })) {
            break;
        }
    }

    const std::string value = _colors->isEmpty() ? "#000000" : _colors->getOrDefault().toString();
    auto css = _sheet->text();
    Inkscape::Css::add_root_custom_property(css, name, value);
    writeCss(std::move(css));

    for (int i = 0; i < static_cast<int>(_swatches.size()); ++i) {
        if (_swatches[i].cssName() == name) {
            _ui->swatchList->setCurrentRow(i);
            auto item = _ui->swatchList->item(i);
            _ui->swatchList->scrollToItem(item);
            _ui->swatchList->editItem(item);
            break;
        }
    }
}

void SwatchEditor::onDelete() {
    if (!_document || _selected < 0 || _selected >= static_cast<int>(_swatches.size())) {
        return;
    }
    const int deleted = _selected;
    auto css = _sheet->text();
    if (Inkscape::Css::remove_root_custom_property(css, _swatches[_selected].cssName())) {
        writeCss(std::move(css));
        // Select whatever slid into the deleted row's slot — the next swatch,
        // or the last one if the tail was deleted. An empty list keeps -1.
        if (!_swatches.empty()) {
            _ui->swatchList->setCurrentRow(std::min<int>(deleted, _swatches.size() - 1));
        }
    }
}

void SwatchEditor::writeCss(std::string&& css) {
    if (!_document) return;

    auto scoped = _update.block();
    // Element creation inside setText fires on_change synchronously; the
    // pending update keeps the local state as the authority.
    _sheet->setText(css);
    Inkscape::DocumentUndo::maybeDone(_document, "document-swatches", RC_("Undo", "Edit document swatches"), "");
    reload();
}

} // namespace Linea::UI
