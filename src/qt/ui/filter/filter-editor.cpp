// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt filter editor widget.
 */

#include "filter-editor.h"
#include "component-transfer-settings-widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QPushButton>
#include <QPoint>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>
#include <set>
#include <sstream>

#include "desktop.h"
#include "document-undo.h"
#include "filter-chemistry.h"
#include "filter-enums.h"
#include "filter-primitive-list.h"
#include "layer-manager.h"
#include "light-source-settings-widget.h"
#include "number-edit.h"
#include "object/filters/componenttransfer.h"
#include "object/filters/diffuselighting.h"
#include "object/filters/specularlighting.h"
#include "object/filters/sp-filter-primitive.h"
#include "preferences.h"
#include "object/sp-filter.h"
#include "object/sp-item.h"
#include "popup-menu.h"
#include "primitive-settings-widget.h"
#include "selection-chemistry.h"
#include "svg-attribute-format.h"
#include "selection.h"
#include "ui/icon-names.h"
#include "ui_filter-editor.h"
#include "xml/repr.h"

using namespace Inkscape;
using namespace Inkscape::Filters;

namespace Linea::UI {

namespace {

QString filter_name(SPFilter* filter) {
    if (!filter) return {};
    if (auto label = filter->label()) return QString::fromUtf8(label);
    if (auto id = filter->getId()) return QString::fromUtf8(id);
    return QObject::tr("filter");
}

QString primitive_name(SPFilterPrimitive* primitive) {
    if (!primitive || !primitive->getRepr()) return {};
    auto id = FPConverter.get_id_from_key(primitive->getRepr()->name());
    return QObject::tr(FPConverter.get_label(id).c_str());
}

QString primitive_category(const QString& element) {
    if (element == QStringLiteral("feBlend") || element == QStringLiteral("feComposite") ||
        element == QStringLiteral("feMerge")) return QObject::tr("Compositing");
    if (element == QStringLiteral("feColorMatrix") || element == QStringLiteral("feComponentTransfer") ||
        element == QStringLiteral("feFlood")) return QObject::tr("Color");
    if (element == QStringLiteral("feDiffuseLighting") || element == QStringLiteral("feSpecularLighting"))
        return QObject::tr("Lighting");
    if (element == QStringLiteral("feGaussianBlur") || element == QStringLiteral("feMorphology") ||
        element == QStringLiteral("feOffset") || element == QStringLiteral("feDisplacementMap"))
        return QObject::tr("Geometry and blur");
    if (element == QStringLiteral("feImage") || element == QStringLiteral("feTile") ||
        element == QStringLiteral("feTurbulence")) return QObject::tr("Sources and texture");
    return QObject::tr("Other");
}

QString primitive_description(const QString& element) {
    if (element == QStringLiteral("feGaussianBlur")) return QObject::tr("Blurs the input using a Gaussian distribution.");
    if (element == QStringLiteral("feColorMatrix")) return QObject::tr("Applies a matrix transformation to the color and alpha channels.");
    if (element == QStringLiteral("feComponentTransfer")) return QObject::tr("Adjusts each color channel independently.");
    if (element == QStringLiteral("feComposite")) return QObject::tr("Combines two input images using a compositing operator.");
    if (element == QStringLiteral("feBlend")) return QObject::tr("Combines two input images using a blending mode.");
    if (element == QStringLiteral("feMerge")) return QObject::tr("Merges multiple input images into one result.");
    if (element == QStringLiteral("feFlood")) return QObject::tr("Fills the filter region with a color and opacity.");
    if (element == QStringLiteral("feImage")) return QObject::tr("Imports an external image or SVG element.");
    if (element == QStringLiteral("feTurbulence")) return QObject::tr("Generates a noise texture.");
    if (element == QStringLiteral("feDisplacementMap")) return QObject::tr("Displaces pixels using a second input image.");
    return QObject::tr("Applies the selected SVG filter operation.");
}

} // namespace

FilterEditor::FilterEditor(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::FilterEditor>()) {
    _ui->setupUi(this);
    _settingsWidget = new PrimitiveSettingsWidget(_ui->primitiveSettingsContainer);
    _componentTransferWidget = new ComponentTransferSettingsWidget(_ui->primitiveSettingsContainer);
    _lightSourceWidget = new LightSourceSettingsWidget(_ui->primitiveSettingsContainer);
    _ui->primitiveSettingsLayout->insertWidget(1, _settingsWidget);
    _ui->primitiveSettingsLayout->insertWidget(2, _componentTransferWidget);
    _ui->primitiveSettingsLayout->insertWidget(3, _lightSourceWidget);
    _settingsWidget->setVisible(false);
    _componentTransferWidget->setVisible(false);
    _lightSourceWidget->setVisible(false);
    _ui->editorSplitter->setStretchFactor(0, 1);
    _ui->editorSplitter->setStretchFactor(1, 0);
    auto prefs = Inkscape::Preferences::get();
    const auto graphHeight = prefs->getIntLimited("/dialogs/filters/handlePos", 440, 240, 9999);
    _ui->editorSplitter->setSizes({graphHeight, 250});
    const auto showSources = prefs->getBool("/dialogs/filters/showAllSources", false);
    _ui->showSourcesButton->setChecked(showSources);
    _ui->primitiveList->setShowAllSources(showSources);
    setupFilterPopup();
    setupFilterOptionsPopup();
    setupEffectInfoPopup();

    setupPrimitiveTypes();
    setupConnections();
    updateFilterControls();
    updatePrimitiveControls();
}

FilterEditor::~FilterEditor() = default;

void FilterEditor::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    const auto desired = width() < 760 ? Qt::Vertical : Qt::Horizontal;
    if (_ui->editorSplitter->orientation() != desired) {
        _ui->editorSplitter->setOrientation(desired);
        _ui->editorSplitter->setSizes(desired == Qt::Vertical ? QList<int>{height() * 2 / 3, height() / 3}
                                                              : QList<int>{width() * 2 / 3, width() / 3});
    }
}

void FilterEditor::setDesktop(SPDesktop* desktop) {
    _selectionChanged.disconnect();
    _selectionModified.disconnect();
    _desktop = desktop;
    _settingsWidget->setDesktop(desktop);
    setDocument(desktop ? desktop->getDocument() : nullptr);
    if (!desktop) {
        setSelection(nullptr);
        return;
    }
    auto selection = desktop->getSelection();
    setSelection(selection);
    _selectionChanged = selection->connectChanged([this](Inkscape::Selection* current) {
        setSelection(current);
    });
    _selectionModified = selection->connectModified([this](Inkscape::Selection* current, unsigned) {
        setSelection(current);
    });
}

void FilterEditor::setDocument(SPDocument* document) {
    _resourceChanged.disconnect();
    _documentDestroyed.disconnect();
    _filterModified.disconnect();
    _primitiveModified.disconnect();
    _document = document;
    _selectedFilter = nullptr;
    _selectedPrimitive = nullptr;
    if (_document) {
        _documentDestroyed = _document->connectDestroy([this] {
            _resourceChanged.disconnect();
            _filterModified.disconnect();
            _primitiveModified.disconnect();
            _selectionChanged.disconnect();
            _selectionModified.disconnect();
            _document = nullptr;
            _desktop = nullptr;
            _selection = nullptr;
            _selectedFilter = nullptr;
            _selectedPrimitive = nullptr;
        });
        _resourceChanged = _document->connectResourcesChanged("filter", [this] {
            if (_document) refreshFilters();
        });
    }
    refreshFilters();
}

void FilterEditor::setSelection(Selection* selection) {
    _selection = selection;
    refreshSelectionState();
}

SPFilter* FilterEditor::selectedFilter() const {
    return _selectedFilter;
}

SPFilterPrimitive* FilterEditor::selectedPrimitive() const {
    return _selectedPrimitive;
}

void FilterEditor::setupPrimitiveTypes() {
    _ui->addPrimitiveCombo->clear();
    QStringList labels;
    QString lastCategory;
    for (int value = 0; value < NR_FILTER_ENDPRIMITIVETYPE; ++value) {
        auto type = static_cast<FilterPrimitiveType>(value);
        auto label = FPConverter.get_label(type);
        if (!label.empty()) {
            const auto key = QString::fromUtf8(FPConverter.get_key(type).c_str());
            if (key == QStringLiteral("svg:feFuncR") || key == QStringLiteral("svg:feFuncG") ||
                key == QStringLiteral("svg:feFuncB") || key == QStringLiteral("svg:feFuncA") ||
                key == QStringLiteral("svg:feMergeNode") || key == QStringLiteral("svg:feDistantLight") ||
                key == QStringLiteral("svg:fePointLight") || key == QStringLiteral("svg:feSpotLight")) {
                continue;
            }
            const auto category = primitive_category(key);
            if (!lastCategory.isEmpty() && category != lastCategory) _ui->addPrimitiveCombo->insertSeparator(
                _ui->addPrimitiveCombo->count());
            if (category != lastCategory) {
                _ui->addPrimitiveCombo->addItem(category);
                const auto categoryIndex = _ui->addPrimitiveCombo->count() - 1;
                _ui->addPrimitiveCombo->setItemData(categoryIndex, 0, Qt::UserRole - 1);
                lastCategory = category;
            }
            auto translated = tr(label.c_str());
            _ui->addPrimitiveCombo->addItem(QIcon(QStringLiteral(":/icons/dialog-filters")), translated, value);
            labels.append(translated);
        }
    }

    auto completer = new QCompleter(labels, _ui->effectSearchEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    _ui->effectSearchEdit->setCompleter(completer);
    connect(completer, QOverload<const QString&>::of(&QCompleter::activated), this, [this](const QString& label) {
        const int index = _ui->addPrimitiveCombo->findText(label);
        if (index < 0) return;
        _ui->addPrimitiveCombo->setCurrentIndex(index);
        addPrimitive();
        _ui->effectSearchEdit->clear();
    });
}

void FilterEditor::setupFilterPopup() {
    _filterPopup = new PopupMenu(this);
    _filterPopup->setFixedSize(600, 650);

    auto content = new QWidget(_filterPopup);
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _filterTable = new QTableWidget(content);
    _filterTable->setColumnCount(2);
    _filterTable->setHorizontalHeaderLabels({tr("Filter"), tr("Used")});
    _filterTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _filterTable->setSelectionMode(QAbstractItemView::SingleSelection);
    _filterTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _filterTable->verticalHeader()->setVisible(false);
    _filterTable->horizontalHeader()->setStretchLastSection(false);
    _filterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _filterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _filterTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_filterTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        auto index = _filterTable->indexAt(position);
        if (!index.isValid() || index.row() >= _ui->filterList->count()) return;
        auto item = _ui->filterList->item(index.row());
        _ui->filterList->setCurrentItem(item);
        setCurrentFilter(item->data(Qt::UserRole).value<SPFilter*>());

        QMenu menu(this);
        auto duplicate = menu.addAction(tr("Duplicate"));
        auto remove = menu.addAction(tr("Remove"));
        auto rename = menu.addAction(tr("Rename"));
        menu.addSeparator();
        auto select = menu.addAction(tr("Select Filter Elements"));
        auto action = menu.exec(_filterTable->viewport()->mapToGlobal(position));
        if (action == duplicate) {
            duplicateFilter();
        } else if (action == remove) {
            deleteFilter();
        } else if (action == rename) {
            bool accepted = false;
            const auto name = QInputDialog::getText(this, tr("Rename Filter"), tr("Filter:"),
                                                     QLineEdit::Normal, item->text(), &accepted);
            if (accepted && !name.isEmpty() && _selectedFilter) {
                _selectedFilter->setLabel(name.toUtf8().constData());
                item->setText(name);
                DocumentUndo::done(_selectedFilter->document, RC_("Undo", "Rename filter"),
                                   INKSCAPE_ICON("dialog-filters"));
                updateFilterControls();
            }
        } else if (action == select) {
            selectFilterUsers();
        }
    });
    layout->addWidget(_filterTable);

    auto footer = new QHBoxLayout();
    auto addActionButton = [this, footer](const char* icon, const char* tip) {
        auto button = new QPushButton(_filterPopup);
        button->setFlat(true);
        button->setIcon(QIcon(QStringLiteral(":/icons/") + QString::fromLatin1(icon)));
        button->setToolTip(tr(tip));
        footer->addWidget(button);
        return button;
    };
    auto newButton = addActionButton("list-add", "Create a new filter");
    _filterDuplicateButton = addActionButton("edit-duplicate", "Duplicate current filter");
    _filterDeleteButton = addActionButton("edit-delete", "Delete current filter");
    _filterSelectButton = addActionButton("object-select", "Select objects that use this filter");
    auto duplicateButton = _filterDuplicateButton;
    auto deleteButton = _filterDeleteButton;
    auto selectButton = _filterSelectButton;
    footer->addStretch();
    layout->addLayout(footer);

    connect(_filterTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row < 0 || row >= _ui->filterList->count()) return;
        auto item = _ui->filterList->item(row);
        _ui->filterList->setCurrentItem(item);
        setCurrentFilter(item->data(Qt::UserRole).value<SPFilter*>());
        _filterPopup->hide();
    });
    connect(newButton, &QPushButton::clicked, this, [this] {
        createFilter();
        _filterPopup->hide();
    });
    connect(duplicateButton, &QPushButton::clicked, this, [this] {
        duplicateFilter();
        _filterPopup->hide();
    });
    connect(deleteButton, &QPushButton::clicked, this, [this] {
        deleteFilter();
        _filterPopup->hide();
    });
    connect(selectButton, &QPushButton::clicked, this, [this] {
        selectFilterUsers();
        _filterPopup->hide();
    });
    connect(_ui->filterSelectorButton, &QPushButton::clicked, this, [this] {
        _filterTable->setRowCount(0);
        for (int index = 0; index < _ui->filterList->count(); ++index) {
            auto filterItem = _ui->filterList->item(index);
            auto filter = filterItem->data(Qt::UserRole).value<SPFilter*>();
            const int row = _filterTable->rowCount();
            _filterTable->insertRow(row);
            _filterTable->setItem(row, 0, new QTableWidgetItem(filterItem->text()));
            _filterTable->setItem(row, 1, new QTableWidgetItem(QString::number(selectedUsageCount(filter))));
            if (filter == _selectedFilter) _filterTable->selectRow(row);
        }
        _filterPopup->showBelowWidget(_ui->filterSelectorButton);
    });
    _filterPopup->setContent(content);
}

void FilterEditor::setupFilterOptionsPopup() {
    _filterOptionsPopup = new PopupMenu(this);
    _filterOptionsPopup->setFixedSize(420, 180);

    auto content = new QWidget(_filterOptionsPopup);
    auto grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);

    _filterAutoRegion = new QCheckBox(tr("Automatic Region"), content);
    _filterX = new NumberEdit(content);
    _filterY = new NumberEdit(content);
    _filterWidth = new NumberEdit(content);
    _filterHeight = new NumberEdit(content);
    _filterX->setLabel(tr("X"));
    _filterY->setLabel(tr("Y"));
    _filterWidth->setLabel(tr("W"));
    _filterHeight->setLabel(tr("H"));
    for (auto edit : {_filterX, _filterY, _filterWidth, _filterHeight}) {
        edit->setRange(-1000.0, 1000.0);
        edit->setDecimals(2);
        edit->setSingleStep(0.01);
    }
    _filterWidth->setMinimum(0.0);
    _filterHeight->setMinimum(0.0);
    int row = 0;
    grid->addWidget(_filterAutoRegion, row++, 0, 1, 2);
    auto label1 = new QLabel(tr("Position"), content);
    label1->setProperty("class", "panel-label");
    grid->addWidget(label1, row++, 0, 1, 2);
    grid->addWidget(_filterX, row, 0);
    grid->addWidget(_filterY, row++, 1);
    auto label2 = new QLabel(tr("Size"), content);
    label2->setProperty("class", "panel-label");
    grid->addWidget(label2, row++, 0, 1, 2);
    grid->addWidget(_filterWidth, row, 0);
    grid->addWidget(_filterHeight, row++, 1);

    auto write = [this](const char* name, const QString& value) {
        if (!_selectedFilter || !_selectedFilter->getRepr()) return;
        const auto data = value.toUtf8();
        _selectedFilter->setAttributeOrRemoveIfEmpty(name, data.constData());
        _selectedFilter->requestModified(SP_OBJECT_MODIFIED_FLAG);
        DocumentUndo::maybeDone(_selectedFilter->document, "filtereffects:region",
                                RC_("Undo", "Set filter attribute"), INKSCAPE_ICON("dialog-filters"));
    };
    connect(_filterAutoRegion, &QCheckBox::toggled, this, [this, write](bool checked) {
        write("inkscape:auto-region", checked ? QStringLiteral("true") : QStringLiteral("false"));
        _filterX->setEnabled(!checked);
        _filterY->setEnabled(!checked);
        _filterWidth->setEnabled(!checked);
        _filterHeight->setEnabled(!checked);
    });
    auto connectNumber = [this, write](NumberEdit* edit, const char* name) {
        connect(edit, &NumberEdit::valueChanged, this,
                [write, name](double value) { write(name, Filter::format_number(value)); });
    };
    connectNumber(_filterX, "x");
    connectNumber(_filterY, "y");
    connectNumber(_filterWidth, "width");
    connectNumber(_filterHeight, "height");

    _filterOptionsPopup->setContent(content);
    connect(_ui->filterOptionsButton, &QPushButton::clicked, this, [this] {
        refreshFilterOptionsPopup();
        _filterOptionsPopup->showBelowWidget(_ui->filterOptionsButton);
    });
}

void FilterEditor::refreshFilterOptionsPopup() {
    if (!_selectedFilter || !_selectedFilter->getRepr()) {
        QSignalBlocker blockAuto(_filterAutoRegion);
        QSignalBlocker blockX(_filterX);
        QSignalBlocker blockY(_filterY);
        QSignalBlocker blockWidth(_filterWidth);
        QSignalBlocker blockHeight(_filterHeight);
        _filterAutoRegion->setChecked(false);
        for (auto edit : {_filterX, _filterY, _filterWidth, _filterHeight}) {
            edit->setValue(0.0);
            edit->setEnabled(false);
        }
        return;
    }
    auto repr = _selectedFilter->getRepr();
    auto read = [repr](const char* name, double fallback) {
        auto value = repr->attribute(name);
        if (!value) return fallback;
        bool valid = false;
        auto number = QString::fromUtf8(value).toDouble(&valid);
        return valid ? number : fallback;
    };
    const bool automatic = repr->attribute("inkscape:auto-region") &&
                           std::strcmp(repr->attribute("inkscape:auto-region"), "false") != 0;
    QSignalBlocker blockAuto(_filterAutoRegion);
    QSignalBlocker blockX(_filterX);
    QSignalBlocker blockY(_filterY);
    QSignalBlocker blockWidth(_filterWidth);
    QSignalBlocker blockHeight(_filterHeight);
    _filterAutoRegion->setChecked(automatic);
    _filterX->setValue(read("x", -0.1));
    _filterY->setValue(read("y", -0.1));
    _filterWidth->setValue(read("width", 1.2));
    _filterHeight->setValue(read("height", 1.2));
    _filterX->setEnabled(!automatic);
    _filterY->setEnabled(!automatic);
    _filterWidth->setEnabled(!automatic);
    _filterHeight->setEnabled(!automatic);
}

void FilterEditor::setupEffectInfoPopup() {
    _effectInfoPopup = new PopupMenu(this);
    _effectInfoPopup->setFixedSize(360, 180);
    auto content = new QWidget(_effectInfoPopup);
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    _effectInfoTitle = new QLabel(content);
    _effectInfoTitle->setProperty("class", "panel-label");
    _effectInfoBody = new QLabel(content);
    _effectInfoBody->setWordWrap(true);
    layout->addWidget(_effectInfoTitle);
    layout->addWidget(_effectInfoBody);
    layout->addStretch();
    _effectInfoPopup->setContent(content);

    connect(_ui->effectInfoButton, &QPushButton::clicked, this, [this] {
        if (!_selectedPrimitive || !_selectedPrimitive->getRepr()) return;
        const auto element = QString::fromUtf8(_selectedPrimitive->getRepr()->name());
        _effectInfoTitle->setText(primitive_name(_selectedPrimitive));
        _effectInfoBody->setText(tr("SVG element: %1\n\n%2").arg(element, primitive_description(element)));
        _effectInfoPopup->showBelowWidget(_ui->effectInfoButton);
    });
}

void FilterEditor::setupConnections() {
    connect(_ui->filterList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                setCurrentFilter(current ? current->data(Qt::UserRole).value<SPFilter*>() : nullptr);
            });
    connect(_ui->primitiveList, &FilterPrimitiveList::primitiveSelected, this,
            [this](SPFilterPrimitive* primitive) { setCurrentPrimitive(primitive); });
    connect(_ui->primitiveList, &FilterPrimitiveList::primitiveChanged, this,
            [this](SPFilterPrimitive*) { refreshPrimitives(); });
    connect(_ui->primitiveList, &FilterPrimitiveList::primitiveContextMenu, this,
            [this](SPFilterPrimitive* primitive, const QPoint& position) {
                if (!primitive || primitive != _selectedPrimitive) setCurrentPrimitive(primitive);
                QMenu menu(this);
                auto duplicate = menu.addAction(tr("Duplicate"));
                auto remove = menu.addAction(tr("Remove"));
                auto action = menu.exec(position);
                if (action == duplicate) duplicatePrimitive();
                else if (action == remove) deletePrimitive();
            });
    connect(_ui->newFilterButton, &QPushButton::clicked, this, &FilterEditor::createFilter);
    connect(_ui->duplicateFilterButton, &QPushButton::clicked, this, &FilterEditor::duplicateFilter);
    connect(_ui->deleteFilterButton, &QPushButton::clicked, this, &FilterEditor::deleteFilter);
    connect(_ui->selectFilterButton, &QPushButton::clicked, this, &FilterEditor::selectFilterUsers);
    connect(_ui->filterEnabledCheckBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        toggleSelectedFilter(state != Qt::Unchecked);
    });
    connect(_ui->effectPopupButton, &QPushButton::clicked, this, [this] { 
        QMenu menu(this);
        const auto query = _ui->effectSearchEdit->text().trimmed();
        for (int index = 0; index < _ui->addPrimitiveCombo->count(); ++index) {
            auto label = _ui->addPrimitiveCombo->itemText(index);
            if (!query.isEmpty() && !label.contains(query, Qt::CaseInsensitive)) continue;
            auto action = menu.addAction(label);
            action->setData(index);
            connect(action, &QAction::triggered, this, [this, action] {
                _ui->addPrimitiveCombo->setCurrentIndex(action->data().toInt());
                addPrimitive();
            });
        }
        menu.exec(_ui->effectPopupButton->mapToGlobal(QPoint(0, _ui->effectPopupButton->height())));
    });
    connect(_ui->effectSearchEdit, &QLineEdit::returnPressed, _ui->effectPopupButton, &QPushButton::click);
    connect(_ui->showSourcesButton, &QPushButton::toggled, _ui->primitiveList, &FilterPrimitiveList::setShowAllSources);
    connect(_ui->showSourcesButton, &QPushButton::toggled, this, [](bool show) {
        Inkscape::Preferences::get()->setBool("/dialogs/filters/showAllSources", show);
    });
    connect(_ui->editorSplitter, &QSplitter::splitterMoved, this, [](int pos, int) {
        Inkscape::Preferences::get()->setInt("/dialogs/filters/handlePos", pos);
    });
    connect(_ui->duplicatePrimitiveButton, &QPushButton::clicked, this, &FilterEditor::duplicatePrimitive);
    connect(_ui->deletePrimitiveButton, &QPushButton::clicked, this, &FilterEditor::deletePrimitive);
    connect(_ui->filterList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (_updating || !item) return;
        auto filter = item->data(Qt::UserRole).value<SPFilter*>();
        if (!filter) return;
        filter->setLabel(item->text().toUtf8().constData());
        DocumentUndo::done(filter->document, RC_("Undo", "Rename filter"), INKSCAPE_ICON("dialog-filters"));
    });
    auto onSettingsChanged = [this](SPFilterPrimitive* primitive, const QString& attribute, const QString&) {
        if (!primitive || !_selectedFilter) return;
        // Block the _primitiveModified callback while we request the update:
        // refreshPrimitiveSettings() would rebuild the form synchronously and
        // delete the widget that is still on the call stack (the spinbox that
        // emitted the signal), causing a use-after-free crash in QWidget::style().
        _updating = true;
        primitive->requestModified(SP_OBJECT_MODIFIED_FLAG);
        _updating = false;
        auto key = QStringLiteral("filtereffects:") + attribute;
        DocumentUndo::maybeDone(primitive->document, key.toUtf8().constData(),
                                RC_("Undo", "Set filter primitive attribute"), INKSCAPE_ICON("dialog-filters"));
        _ui->primitiveList->update();
    };
    connect(_settingsWidget, &PrimitiveSettingsWidget::primitiveChanged, this, onSettingsChanged);
    connect(_componentTransferWidget, &ComponentTransferSettingsWidget::primitiveChanged, this, onSettingsChanged);
    connect(_lightSourceWidget, &LightSourceSettingsWidget::primitiveChanged, this, onSettingsChanged);
}

void FilterEditor::refreshFilters() {
    if (_updating) return;
    _updating = true;
    auto previous = _selectedFilter;
    _ui->filterList->clear();
    _selectedFilter = nullptr;
    _selectedPrimitive = nullptr;
    QListWidgetItem* target = nullptr;

    if (_document) {
        for (auto object : _document->getResourceList("filter")) {
            auto filter = cast<SPFilter>(object);
            if (!filter) continue;
            auto item = new QListWidgetItem(filter_name(filter), _ui->filterList);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            item->setData(Qt::UserRole, QVariant::fromValue(filter));
            if (filter == previous) target = item;
        }
    }

    if (!target && _ui->filterList->count() > 0) {
        target = _ui->filterList->item(0);
    }
    _updating = false;
    if (target) {
        _ui->filterList->setCurrentItem(target);
        setCurrentFilter(target->data(Qt::UserRole).value<SPFilter*>());
    }
    refreshSelectionState();
    updateFilterControls();
}

int FilterEditor::selectedUsageCount(SPFilter* filter) const {
    if (!_selection || !filter) return 0;
    int count = 0;
    for (auto object : _selection->items()) {
        if (object && object->style && object->style->filter.set && object->style->getFilter() == filter) {
            ++count;
        }
    }
    return count;
}

void FilterEditor::selectFilterInList(SPFilter* filter) {
    if (!filter) return;
    for (int index = 0; index < _ui->filterList->count(); ++index) {
        auto item = _ui->filterList->item(index);
        if (item->data(Qt::UserRole).value<SPFilter*>() == filter) {
            _ui->filterList->setCurrentItem(item);
            return;
        }
    }
}

void FilterEditor::refreshSelectionState() {
    if (!_selection) {
        updateFilterControls();
        return;
    }

    std::set<SPFilter*> used;
    int total = 0;
    for (auto object : _selection->items()) {
        ++total;
        if (object && object->style && object->style->filter.set) {
            if (auto filter = object->style->getFilter()) used.insert(filter);
        }
    }
    if (used.size() == 1 && *used.begin() != _selectedFilter) {
        _selectedFilter = *used.begin();
        selectFilterInList(_selectedFilter);
        refreshPrimitives();
    }

    const auto selected = _selectedFilter;
    const int users = selectedUsageCount(selected);
    {
        QSignalBlocker blocker(_ui->filterEnabledCheckBox);
        const auto state = !selected || users == 0 ? Qt::Unchecked
                          : users == total ? Qt::Checked
                                            : Qt::PartiallyChecked;
        _ui->filterEnabledCheckBox->setCheckState(state);
    }
    updateFilterControls();
}

void FilterEditor::refreshPrimitives() {
    _updating = true;
    _ui->primitiveList->setFilter(_selectedFilter);
    _selectedPrimitive = _ui->primitiveList->selectedPrimitive();
    _updating = false;
    refreshPrimitiveSettings();
    updatePrimitiveControls();
}

void FilterEditor::refreshPrimitiveSettings() {
    const bool hasPrimitive = _selectedPrimitive != nullptr;
    const bool componentTransfer = is<SPFeComponentTransfer>(_selectedPrimitive);
    const bool lightSource = is<SPFeDiffuseLighting>(_selectedPrimitive) || is<SPFeSpecularLighting>(_selectedPrimitive);
    _ui->effectNameLabel->setText(hasPrimitive ? primitive_name(_selectedPrimitive) : QString());
    _settingsWidget->setVisible(hasPrimitive && !componentTransfer && !lightSource);
    _componentTransferWidget->setVisible(componentTransfer);
    _lightSourceWidget->setVisible(lightSource);
    _ui->primitiveSettingsLabel->setVisible(!hasPrimitive);

    if (componentTransfer) {
        _componentTransferWidget->setPrimitive(_selectedPrimitive);
    } else if (lightSource) {
        _lightSourceWidget->setPrimitive(_selectedPrimitive);
    } else if (hasPrimitive) {
        _settingsWidget->setPrimitive(_selectedPrimitive);
    } else {
        const auto message = !_document ? tr("No document")
                           : _ui->filterList->count() == 0 ? tr("No filters in document")
                                                           : (_selectedFilter ? tr("Select an effect") : tr("Select a filter"));
        _ui->primitiveSettingsLabel->setText(message);
    }
}

void FilterEditor::setCurrentFilter(SPFilter* filter) {
    if (_updating || filter == _selectedFilter) return;
    _filterModified.disconnect();
    _primitiveModified.disconnect();
    _selectedFilter = filter;
    _selectedPrimitive = nullptr;
    if (_selectedFilter) {
        _filterModified = _selectedFilter->connectModified([this](auto, auto) {
            if (!_updating) refreshPrimitives();
        });
    }
    refreshPrimitives();
    refreshSelectionState();
    updateFilterControls();
}

void FilterEditor::setCurrentPrimitive(SPFilterPrimitive* primitive) {
    if (_updating || primitive == _selectedPrimitive) return;
    _primitiveModified.disconnect();
    _selectedPrimitive = primitive;
    if (_selectedPrimitive) {
        _primitiveModified = _selectedPrimitive->connectModified([this](auto, auto) {
            if (!_updating) refreshPrimitiveSettings();
        });
    }
    refreshPrimitiveSettings();
    updatePrimitiveControls();
}

void FilterEditor::createFilter() {
    if (!_document) return;
    auto filter = new_filter(_document);
    std::ostringstream name;
    name << tr("filter").toStdString() << _ui->filterList->count();
    filter->setLabel(name.str().c_str());
    refreshFilters();
    for (int index = 0; index < _ui->filterList->count(); ++index) {
        auto item = _ui->filterList->item(index);
        if (item->data(Qt::UserRole).value<SPFilter*>() == filter) {
            _ui->filterList->setCurrentItem(item);
            break;
        }
    }
    DocumentUndo::done(_document, RC_("Undo", "Add filter"), INKSCAPE_ICON("dialog-filters"));
}

void FilterEditor::duplicateFilter() {
    if (!_selectedFilter || !_selectedFilter->getRepr()) return;
    auto parent = _selectedFilter->getRepr()->parent();
    if (!parent) return;
    auto repr = _selectedFilter->getRepr()->duplicate(_selectedFilter->getRepr()->document());
    parent->appendChild(repr);
    DocumentUndo::done(_selectedFilter->document, RC_("Undo", "Duplicate filter"), INKSCAPE_ICON("dialog-filters"));
    refreshFilters();
}

void FilterEditor::deleteFilter() {
    if (!_selectedFilter || !_desktop) return;
    auto filter = _selectedFilter;
    auto document = filter->document;
    auto all = get_all_items(_desktop->layerManager().currentRoot(), _desktop, false, false, true);
    for (auto item : all) {
        if (!item || !item->style) continue;
        if (item->style->filter.href && item->style->filter.href->getObject() == filter) {
            ::remove_filter(item, false);
        }
    }
    sp_repr_unparent(filter->getRepr());
    DocumentUndo::done(document, RC_("Undo", "Remove filter"), INKSCAPE_ICON("dialog-filters"));
    refreshFilters();
}

void FilterEditor::selectFilterUsers() {
    if (!_selectedFilter || !_desktop) return;
    std::vector<SPItem*> items;
    auto all = get_all_items(_desktop->layerManager().currentRoot(), _desktop, false, false, true);
    for (auto item : all) {
        if (!item || !item->style) continue;
        if (item->style->filter.href && item->style->filter.href->getObject() == _selectedFilter) {
            items.push_back(item);
        }
    }
    _desktop->getSelection()->setList(items);
}

void FilterEditor::toggleSelectedFilter(bool active) {
    if (_updating || !_selectedFilter || !_selection || !_document) return;
    for (auto item : _selection->items()) {
        if (!item) continue;
        if (active && _selectedFilter->valid_for(item)) {
            sp_style_set_property_url(item, "filter", _selectedFilter, false);
        } else {
            ::remove_filter(item, false);
        }
        item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
    }
    DocumentUndo::done(_document, RC_("Undo", "Apply filter"), INKSCAPE_ICON("dialog-filters"));
    refreshSelectionState();
}

void FilterEditor::addPrimitive() {
    if (!_selectedFilter) return;
    auto value = _ui->addPrimitiveCombo->currentData();
    if (!value.isValid()) return;
    auto type = static_cast<FilterPrimitiveType>(value.toInt());
    auto primitive = filter_add_primitive(_selectedFilter, type);
    DocumentUndo::done(_selectedFilter->document, RC_("Undo", "Add filter primitive"), INKSCAPE_ICON("dialog-filters"));
    refreshPrimitives();
    _ui->primitiveList->selectPrimitive(primitive);
    _selectedPrimitive = _ui->primitiveList->selectedPrimitive();
    refreshPrimitiveSettings();
    updatePrimitiveControls();
}

void FilterEditor::duplicatePrimitive() {
    if (!_selectedFilter || !_selectedPrimitive || !_selectedPrimitive->getRepr()) return;
    auto repr = _selectedPrimitive->getRepr()->duplicate(_selectedPrimitive->getRepr()->document());
    _selectedFilter->getRepr()->appendChild(repr);
    DocumentUndo::done(_selectedFilter->document, RC_("Undo", "Duplicate filter primitive"),
                       INKSCAPE_ICON("dialog-filters"));
    refreshPrimitives();
}

void FilterEditor::deletePrimitive() {
    if (!_selectedPrimitive) return;
    auto primitive = _selectedPrimitive;
    sp_repr_unparent(primitive->getRepr());
    DocumentUndo::done(primitive->document, RC_("Undo", "Remove filter primitive"), INKSCAPE_ICON("dialog-filters"));
    refreshPrimitives();
}

void FilterEditor::updateFilterControls() {
    const bool hasFilter = _selectedFilter != nullptr;
    const auto filterText = hasFilter ? filter_name(_selectedFilter)
                          : !_document ? tr("No document")
                                       : _ui->filterList->count() == 0 ? tr("No filters") : QStringLiteral("-");
    _ui->filterSelectorButton->setText(filterText);
    _ui->filterSelectorButton->setEnabled(_ui->filterList->count() > 0);
    _ui->duplicateFilterButton->setEnabled(hasFilter);
    _ui->deleteFilterButton->setEnabled(hasFilter);
    _ui->selectFilterButton->setEnabled(hasFilter);
    _filterDuplicateButton->setEnabled(hasFilter);
    _filterDeleteButton->setEnabled(hasFilter);
    _filterSelectButton->setEnabled(hasFilter);
    _ui->filterEnabledCheckBox->setEnabled(hasFilter && _selection != nullptr);
    _ui->filterOptionsButton->setEnabled(hasFilter);
}

void FilterEditor::updatePrimitiveControls() {
    const bool hasPrimitive = _selectedPrimitive != nullptr;
    const bool hasFilter = _selectedFilter != nullptr;
    _ui->effectSearchEdit->setEnabled(hasFilter);
    _ui->effectPopupButton->setEnabled(hasFilter);
    _ui->duplicatePrimitiveButton->setEnabled(hasPrimitive);
    _ui->deletePrimitiveButton->setEnabled(hasPrimitive);
    _ui->showSourcesButton->setEnabled(hasFilter);
}

} // namespace Linea::UI
