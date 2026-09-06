// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * MeshEditor — Qt mesh editor widget for Fill and Stroke panel.
 *
 * Copyright (C) 2024-2026 Michael Kowalski
 */

#ifndef LINEA_UI_MESH_EDITOR_H
#define LINEA_UI_MESH_EDITOR_H

#include <QWidget>
#include <memory>
#include <string>
#include <vector>

#include <sigc++/scoped_connection.h>

#include "2geom/int-rect.h"
#include "ui/operation-blocker.h"

class SPDocument;
class SPGradient;
class SPMeshGradient;

QT_BEGIN_NAMESPACE
namespace Ui { class MeshEditorUi; }
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Mesh editor widget providing mesh gradient selection from document.
 * Displays mesh gradients in a grid with preview images.
 */
class MeshEditor : public QWidget {
    Q_OBJECT

public:
    explicit MeshEditor(QWidget* parent = nullptr);
    ~MeshEditor() override;

    // Pass current document to extract mesh gradients from
    void setDocument(SPDocument* document);

    // Set the selected mesh gradient
    void setSelectedMesh(SPGradient* mesh);

    // Query selected mesh gradient
    SPGradient* getSelectedMesh() const;

Q_SIGNALS:
    // Emitted when the selected mesh gradient changes
    void changed(SPGradient* mesh);
    // Emitted when edit on canvas is requested
    void editRequested();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    struct MeshItem {
        std::string id;
        std::string label;
        std::shared_ptr<QImage> image;
        SPMeshGradient* object;
    };

    using MeshItemPtr = std::shared_ptr<MeshItem>;

    void setupCustomWidgets();
    void connectSignals();

    // Rebuild the list of mesh gradients from document
    std::vector<SPMeshGradient*> rebuildMeshList();

    // Update the grid with mesh items
    void rebuildMeshGrid(const std::vector<SPMeshGradient*>& list);

    // Schedule an update (debounced)
    void scheduleUpdate();

    // Perform the actual update
    void updateMeshList();

    // Selection helpers
    void selectMeshById(const std::string& id);

    // Draw callback for grid cells
    void drawMeshCell(QPainter* painter, uint32_t index, const Geom::IntRect& rect, bool selected);

    // Tooltip callback for grid cells
    QString getMeshTooltip(int index);

    // UI from .ui file
    std::unique_ptr<Ui::MeshEditorUi> _ui;

    // State
    SPDocument* _document = nullptr;
    SPGradient* _selectedMesh = nullptr;
    std::string _selectedId;
    OperationBlocker _update;

    // Mesh items
    std::vector<MeshItemPtr> _meshItems;
    int _selectedCell = -1;

    // Update scheduling
    bool _meshesChanged = false;
    int _updateTimerId = 0;

    // Document signal connections
    sigc::scoped_connection _gradientsConnection;
    sigc::scoped_connection _defsConnection;
};

} // namespace Linea::UI

#endif // LINEA_UI_MESH_EDITOR_H
