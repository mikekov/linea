// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Text panel — font, typography and text decoration editor
 */

#ifndef LINEA_UI_TEXT_PANEL_H
#define LINEA_UI_TEXT_PANEL_H

#include <memory>
#include <vector>
#include <QPushButton>
#include <QWidget>
#include <glibmm/ustring.h>
#include <sigc++/scoped_connection.h>

#include "ui/operation-blocker.h"
#include "util/text-utils.h"

namespace Inkscape { struct FontInfo; }

class SPDesktop;
class SPDocument;
class SPItem;
class SPObject;
class SPText;
class SPCSSAttr;

QT_BEGIN_NAMESPACE
namespace Ui { class TextPanel; }
QT_END_NAMESPACE

namespace Inkscape::Util { class Unit; }
namespace Inkscape::UI::Tools { class TextTool; }

namespace Linea::Props { class Binder; }

namespace Linea::UI {

class DecorationOptions;
class FontList;
class PopupMenu;
class UnitTracker;

/**
 * Text properties panel.
 *
 * Logic for querying and applying text CSS, font discovery,
 * unit conversion and decoration handling.
 */
class TextPanel : public QWidget {
    Q_OBJECT

public:
    explicit TextPanel(QWidget* parent = nullptr);
    ~TextPanel() override;

    void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop);

    /// Declarative binding (called by AppearancePanel after creating the Binder).
    void bind(Props::Binder& binder);

    /// Call when the text tool subselection changes.
    // void subselectionChanged(const std::vector<SPItem*>& items);

    void updateTypographyState(const TypographyState& state, Inkscape::UI::Tools::TextTool* text_tool);

private:
    void setupConnections();
    void setupUnitTrackers();
    void setupFontDiscovery();

    void updateTypographyState();

    // query helpers
    std::vector<SPItem*> getSubselection();
    std::vector<SPItem*> getQueryItems();
    Inkscape::UI::Tools::TextTool* getTextTool();

    // font helpers
    void populateFamilies();
    void populateStyles(int family_data_index);
    void updateFontVariants(const Inkscape::FontInfo* font);
    int findFamilyIndex(const Glib::ustring& name) const;
    int findStyleIndex(const Glib::ustring& name) const;
    const Inkscape::FontInfo* getSelectedFont() const;
    const Inkscape::FontInfo* findFontByStyle(const Glib::ustring& style_name) const;

    bool canUpdate() const;

    std::unique_ptr<Ui::TextPanel> _ui;
    UnitTracker* _tracker_fs = nullptr;
    UnitTracker* _tracker_lh = nullptr;

    SPDocument* _document = nullptr;
    SPDesktop* _desktop = nullptr;
    SPItem* _current_item = nullptr;

    std::vector<std::vector<Inkscape::FontInfo>> _font_families;
    std::vector<Glib::ustring> _family_names;
    std::vector<Glib::ustring> _style_names;
    int _style_index = -2;
    // Tracks the line-height unit for the unitChanged handler's relative↔absolute
    // conversion. Updated by the handler itself (not by the binder push path).
    // See the TODO comment at the unitChanged handler in text-panel.cpp.
    int _lineheight_unit = 0;

    sigc::scoped_connection _font_stream;
    // sigc::scoped_connection _cursor_moved;
    OperationBlocker _update;
    int _variableFontsMaxHeight = 0;
    DecorationOptions* _decorationOptions = nullptr;
    PopupMenu* _decorationPopup = nullptr;
    FontList* _fontList = nullptr;
    PopupMenu* _fontListPopup = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_TEXT_PANEL_H
