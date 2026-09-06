// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt filter editor widget.
 */

#ifndef LINEA_UI_FILTER_FILTER_EDITOR_H
#define LINEA_UI_FILTER_FILTER_EDITOR_H

#include <QWidget>
#include <memory>
#include <sigc++/connection.h>

class SPDesktop;
class SPDocument;
class SPFilter;
class SPFilterPrimitive;
class QCheckBox;
class QPushButton;
class QLabel;
class QResizeEvent;
class QTableWidget;

namespace Linea::UI {
class PopupMenu;
class PrimitiveSettingsWidget;
class ComponentTransferSettingsWidget;
class LightSourceSettingsWidget;
class NumberEdit;
}

namespace Inkscape {
class Selection;
}

namespace Ui {
class FilterEditor;
}

namespace Linea::UI {

class FilterEditor : public QWidget {
    Q_OBJECT

public:
    explicit FilterEditor(QWidget* parent = nullptr);
    ~FilterEditor() override;

    void setDesktop(SPDesktop* desktop);
    void setDocument(SPDocument* document);
    void setSelection(Inkscape::Selection* selection);
    SPFilter* selectedFilter() const;
    SPFilterPrimitive* selectedPrimitive() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupPrimitiveTypes();
    void setupConnections();
    void setupFilterPopup();
    void setupFilterOptionsPopup();
    void setupEffectInfoPopup();
    void refreshFilterOptionsPopup();
    void refreshFilters();
    void refreshSelectionState();
    int selectedUsageCount(SPFilter* filter) const;
    void selectFilterInList(SPFilter* filter);
    void refreshPrimitives();
    void refreshPrimitiveSettings();
    void setCurrentFilter(SPFilter* filter);
    void setCurrentPrimitive(SPFilterPrimitive* primitive);
    void createFilter();
    void duplicateFilter();
    void deleteFilter();
    void selectFilterUsers();
    void toggleSelectedFilter(bool active);
    void addPrimitive();
    void duplicatePrimitive();
    void deletePrimitive();
    void updateFilterControls();
    void updatePrimitiveControls();

    std::unique_ptr<Ui::FilterEditor> _ui;
    SPDesktop* _desktop = nullptr;
    SPDocument* _document = nullptr;
    Inkscape::Selection* _selection = nullptr;
    SPFilter* _selectedFilter = nullptr;
    SPFilterPrimitive* _selectedPrimitive = nullptr;
    bool _updating = false;
    sigc::connection _resourceChanged;
    sigc::connection _documentDestroyed;
    sigc::connection _filterModified;
    sigc::connection _primitiveModified;
    sigc::connection _selectionChanged;
    sigc::connection _selectionModified;
    PrimitiveSettingsWidget* _settingsWidget = nullptr;
    ComponentTransferSettingsWidget* _componentTransferWidget = nullptr;
    LightSourceSettingsWidget* _lightSourceWidget = nullptr;
    PopupMenu* _filterPopup = nullptr;
    QTableWidget* _filterTable = nullptr;
    QPushButton* _filterDuplicateButton = nullptr;
    QPushButton* _filterDeleteButton = nullptr;
    QPushButton* _filterSelectButton = nullptr;
    PopupMenu* _filterOptionsPopup = nullptr;
    PopupMenu* _effectInfoPopup = nullptr;
    QLabel* _effectInfoTitle = nullptr;
    QLabel* _effectInfoBody = nullptr;
    QCheckBox* _filterAutoRegion = nullptr;
    NumberEdit* _filterX = nullptr;
    NumberEdit* _filterY = nullptr;
    NumberEdit* _filterWidth = nullptr;
    NumberEdit* _filterHeight = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_FILTER_EDITOR_H
