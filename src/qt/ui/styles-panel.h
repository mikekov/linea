// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Styles panel — editor for the document's global CSS stylesheet.
 */

#ifndef LINEA_UI_STYLES_PANEL_H
#define LINEA_UI_STYLES_PANEL_H

#include <QWidget>
#include <memory>
#include <sigc++/scoped_connection.h>

#include "syntax.h"
#include "ui/operation-blocker.h"

class SPDocument;
class SPDesktop;

QT_BEGIN_NAMESPACE
namespace Ui {
class StylesPanel;
}
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Panel showing the content of the first <svg:style> element under the
 * document root and editing it as CSS.
 *
 * The panel always shows its editor, even when the document has no style
 * element yet; the element is only inserted into the document when the user
 * edits the stylesheet content.
 */
class StylesPanel : public QWidget {
    Q_OBJECT

public:
    explicit StylesPanel(QWidget* parent = nullptr);
    ~StylesPanel() override;

    void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void restorePanePosition();
    // Refresh the editor from the stylesheet text, unless the user
    // has unsaved typing.
    void onStyleContentChanged();
    void onEditorTextChanged();

    std::unique_ptr<Ui::StylesPanel> _ui;
    SPDocument* _document = nullptr;
    sigc::scoped_connection _stylesheetChanged;
    std::unique_ptr<Syntax::TextEditView> _editor;
    OperationBlocker _update;
    int _savedPanePosition = -1;
    bool _panePositionRestored = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_STYLES_PANEL_H
