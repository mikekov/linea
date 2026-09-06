// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * MarkerComboBox — button with popup for selecting stroke markers
 */

#ifndef LINEA_UI_MARKER_COMBO_BOX_H
#define LINEA_UI_MARKER_COMBO_BOX_H

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <QWidget>
#include <sigc++/scoped_connection.h>

#include "ui/operation-blocker.h"

class SPDesktop;
class SPDocument;
class SPMarker;
class SPObject;
class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class MarkerPopup;
}
QT_END_NAMESPACE

namespace Linea::UI {

class PopupMenu;
class SimpleGrid;

/**
 * ComboBox-like widget for selecting stroke markers.
 *
 * Shows a button with the current marker preview. Clicking it opens a popup
 * with a grid of available markers and property editors (size, orientation,
 * offset, opacity).
 */
class MarkerComboBox : public QWidget {
    Q_OBJECT

public:
    MarkerComboBox(const std::string& id, int loc, QWidget* parent = nullptr);
    ~MarkerComboBox() override;

    void setDocument(SPDocument* doc);
    void setCurrent(SPObject* marker);

    // Get active marker URI and ID
    std::pair<std::string, std::string> getActiveMarkerUri();

    bool inUpdate() const { return _update.pending(); }
    const std::string& getId() const { return _comboId; }
    int getLoc() const { return _loc; }

    QPushButton* button() { return _button; }

Q_SIGNALS:
    void markerChanged();
    void editRequested();

private:
    struct MarkerItem {
        SPDocument* source = nullptr;
        std::string id;
        std::string label;
        bool stock = false;
        bool history = false;
    };

    void construct();
    void setupIcons();
    void connectSignals();
    void buildPopup();

    // Marker list management
    void initMarkerList();
    void markerListFromDoc(SPDocument* source, bool history);
    std::vector<SPMarker*> getMarkerList(SPDocument* source);
    void addMarkers(const std::vector<SPMarker*>& markers, SPDocument* source, bool history);
    void rebuildStore();

    // UI update
    void updateButtonPreview();
    void updatePreview();
    void updateMarkerName();
    void updateWidgetsFromMarker(SPMarker* marker);
    void updateScaleLinkIcon();
    void refreshAfterModified();

    // Selection
    const MarkerItem* getSelectedItem() const;
    SPMarker* getCurrentMarker() const;
    SPMarker* getActiveDocumentMarker();
    int findMarkerIndex(SPMarker* marker) const;
    void selectMarkerAt(int index);

    // Rendering
    QImage renderMarker(SPMarker* marker, int width, int height, double scale = 1.5, std::optional<uint32_t> checkerboard = {});
    QImage renderNoMarker(int width, int height);

    // Marker modification
    void setScale(bool changeWidth);
    void setOrientation(bool enableAngle, const char* value);

    // Data
    std::string _comboId;
    int _loc;
    SPDocument* _document = nullptr;
    std::string _currentMarkerId;
    std::unique_ptr<SPDocument> _sandbox;
    std::vector<MarkerItem> _stockItems;
    std::vector<MarkerItem> _historyItems;
    std::vector<MarkerItem> _allItems; // combined: history + stock
    bool _scaleLinked = true;
    bool _isUpToDate = false;

    // UI
    std::unique_ptr<Ui::MarkerPopup> _popupUi;
    QPushButton* _button = nullptr;
    PopupMenu* _popup = nullptr;
    SimpleGrid* _markerGrid = nullptr;
    QWidget* _popupContent = nullptr;
    OperationBlocker _update;

    // Connections
    sigc::scoped_connection _modifiedConnection;
};

} // namespace Linea::UI

#endif // LINEA_UI_MARKER_COMBO_BOX_H
