// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * MeshEditor — Qt mesh editor widget implementation.
 *
 * Copyright (C) 2024-2026 Michael Kowalski
 */

#include "mesh-editor.h"

#include <QTimer>
#include <QPainter>
#include <QImage>
#include <QTimerEvent>
#include <QGuiApplication>
#include <QScreen>

#include "document.h"
#include "object/sp-defs.h"
#include "object/sp-object.h"
#include "object/sp-mesh-gradient.h"
#include "util/object-renderer.h"
#include "util/cast.h"
#include "ui/util.h"

#include "2geom/int-rect.h"

#include "ui_mesh-editor.h"

namespace Linea::UI {

namespace {

// Helper to get object ID
auto get_id = [](const SPObject* object) {
    auto id = object->getId();
    return id ? id : "";
};

// Helper to format label
auto label_fmt = [](const char* label, const std::string& id) {
    return label && *label ? label : '#' + id;
};

} // namespace

// ─── Constructor / destructor ────────────────────────────────────────────────

MeshEditor::MeshEditor(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::MeshEditorUi>())
{
    _ui->setupUi(this);

    setupCustomWidgets();
    connectSignals();
}

MeshEditor::~MeshEditor() {
    if (_updateTimerId) {
        killTimer(_updateTimerId);
        _updateTimerId = 0;
    }
}

// ─── Setup ───────────────────────────────────────────────────────────────────

void MeshEditor::setupCustomWidgets() {
    // Configure the grid for mesh display
    _ui->meshGrid->setCellSize(34, 34); // 30x30 mesh + 4px gap
    _ui->meshGrid->setGap(1, 1);
    _ui->meshGrid->setCellStretch(false);
    _ui->meshGrid->setHasFrame(true);
    _ui->meshGrid->setSelectable(true);

    // Set draw callback for rendering mesh cells
    _ui->meshGrid->setDrawFunc([this](QPainter* painter, uint32_t index, const Geom::IntRect& rect, bool selected) {
        drawMeshCell(painter, index, rect, selected);
    });

    // Set tooltip callback
    _ui->meshGrid->setTooltipFunc([this](int index) {
        return getMeshTooltip(index);
    });

    // Connect grid selection signal
    connect(_ui->meshGrid, &SimpleGrid::cellSelected, [this](int index) {
        if (_update.pending() || !_document) return;

        _selectedCell = index;
        if (index >= 0 && index < static_cast<int>(_meshItems.size())) {
            auto& item = _meshItems[index];
            if (auto mesh = cast<SPMeshGradient>(_document->getObjectById(item->id))) {
                _selectedMesh = mesh;
                _selectedId = item->id;
                Q_EMIT changed(mesh);
            }
        }
    });

    // Connect edit button
    connect(_ui->editButton, &QPushButton::clicked, [this] {
        Q_EMIT editRequested();
    });
}

void MeshEditor::connectSignals() {
    // Signals are connected in setupCustomWidgets
}

// ─── Document management ──────────────────────────────────────────────────────

void MeshEditor::setDocument(SPDocument* document) {
    if (_document == document) return;

    // Kill any pending debounce timer before changing _document so the
    // timerEvent callback can never fire against a stale pointer.
    if (_updateTimerId) {
        killTimer(_updateTimerId);
        _updateTimerId = 0;
    }
    _meshesChanged = false;

    _gradientsConnection.disconnect();
    _defsConnection.disconnect();
    _document = document;

    if (!_document) {
        // Clear the grid when no document is set
        _meshItems.clear();
        _ui->meshGrid->setCellCount(0);
        return;
    }

    // Connect to document resource changes
    _gradientsConnection = _document->connectResourcesChanged("gradient", [this]() {
        _meshesChanged = true;
        scheduleUpdate();
    });

    // Connect to defs modifications
    _defsConnection = _document->getDefs()->connectModified([this](SPObject* obj, unsigned flags) {
        auto mesh = cast<SPMeshGradient>(obj);
        if (mesh && mesh->getArray() == mesh && (flags & SP_OBJECT_CHILD_MODIFIED_FLAG)) {
            _meshesChanged = true;
            scheduleUpdate();
        }
    });

    _meshesChanged = true;
    scheduleUpdate();
}

// ─── Selection management ─────────────────────────────────────────────────────

void MeshEditor::setSelectedMesh(SPGradient* mesh) {
    _selectedMesh = mesh;
    _selectedId = mesh ? get_id(mesh) : "";

    if (!mesh) return;

    selectMeshById(_selectedId);
}

SPGradient* MeshEditor::getSelectedMesh() const {
    if (!_document) return nullptr;

    if (_selectedCell >= 0 && _selectedCell < static_cast<int>(_meshItems.size())) {
        auto& item = _meshItems[_selectedCell];
        return cast<SPGradient>(_document->getObjectById(item->id));
    }

    // If nothing selected, return first mesh if available
    if (!_meshItems.empty()) {
        auto& item = _meshItems[0];
        return cast<SPGradient>(_document->getObjectById(item->id));
    }

    return nullptr;
}

void MeshEditor::selectMeshById(const std::string& id) {
    // Find the mesh with the given ID and select it
    for (size_t i = 0; i < _meshItems.size(); ++i) {
        if (_meshItems[i]->id == id) {
            auto scoped(_update.block());
            _selectedCell = static_cast<int>(i);
            _ui->meshGrid->invalidate();
            break;
        }
    }
}

// ─── Mesh list management ────────────────────────────────────────────────────

std::vector<SPMeshGradient*> MeshEditor::rebuildMeshList() {
    std::vector<SPMeshGradient*> list;
    if (!_document) return list;

    auto gradients = _document->getResourceList("gradient");

    for (auto obj : gradients) {
        // Collect root mesh gradients only
        if (auto mesh = cast<SPMeshGradient>(obj); mesh && mesh->getArray() == mesh) {
            list.push_back(mesh);
        }
    }

    return list;
}

void MeshEditor::rebuildMeshGrid(const std::vector<SPMeshGradient*>& list) {
    object_renderer renderer;
    _meshItems.clear();

    // Mesh preview size
    const int width = 30;
    const int height = 30;
    auto device_scale = QGuiApplication::primaryScreen() ?
                        QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    object_renderer::options opt = {};

    for (auto mesh : list) {
        auto id = get_id(mesh);
        auto labelstr = mesh->getAttribute("inkscape:label");
        auto label = label_fmt(labelstr, id);

        // Render mesh preview
        auto qimage = renderer.render(*mesh, width, height, device_scale, opt);
        auto image = std::make_shared<QImage>(qimage);

        auto item = std::make_shared<MeshItem>();
        item->id = id;
        item->label = label;
        item->image = image;
        item->object = mesh;

        _meshItems.push_back(item);
    }

    _ui->meshGrid->setCellCount(_meshItems.size());

    // Restore selection if possible
    if (!_selectedId.empty()) {
        selectMeshById(_selectedId);
    }
}

// ─── Update scheduling ─────────────────────────────────────────────────────────

void MeshEditor::scheduleUpdate() {
    if (_updateTimerId) return;

    _updateTimerId = startTimer(100); // 100ms delay
}

void MeshEditor::timerEvent(QTimerEvent* event) {
    if (event->timerId() == _updateTimerId) {
        killTimer(_updateTimerId);
        _updateTimerId = 0;
        updateMeshList();
    }
    QWidget::timerEvent(event);
}

void MeshEditor::updateMeshList() {
    if (!_meshesChanged) return;

    auto list = rebuildMeshList();
    rebuildMeshGrid(list);
    _meshesChanged = false;
}

// ─── Drawing callbacks ────────────────────────────────────────────────────────

void MeshEditor::drawMeshCell(QPainter* painter, uint32_t index, const Geom::IntRect& rect, bool selected) {
    if (index >= _meshItems.size()) return;

    auto& item = _meshItems[index];
    if (!item->image) return;

    // Draw the mesh preview image
    QRect qrect(rect.left(), rect.top(), rect.width(), rect.height());
    QPixmap pixmap = QPixmap::fromImage(*item->image);
    painter->drawPixmap(qrect, pixmap);

    // Draw selection highlight
    if (selected) {
        QPen pen(QColor(0, 0, 255), 2);
        painter->setPen(pen);
        painter->drawRect(qrect.adjusted(0, 0, -1, -1));
    }
}

QString MeshEditor::getMeshTooltip(int index) {
    if (index < 0 || index >= static_cast<int>(_meshItems.size())) return QString();

    auto& item = _meshItems[index];
    return QString::fromStdString(item->label);
}

} // namespace Linea::UI
