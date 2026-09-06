// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Guides panel — show/lock guides and guide colours (Qt version).
 */

#ifndef LINEA_UI_GUIDES_PANEL_H
#define LINEA_UI_GUIDES_PANEL_H

#include <memory>
#include <QWidget>
#include <sigc++/scoped_connection.h>

#include "ui/operation-blocker.h"

class SPDocument;
class SPGuide;
class SPNamedView;

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
namespace Ui {
class GuidesPanel;
}
QT_END_NAMESPACE

namespace Linea::UI {

class GuideWidget;

/**
 * Passive panel for guide visibility, locking, and colour settings.
 *
 * update(SPNamedView*) reads guide state directly from the namedview.
 * Write callbacks also go straight to the namedview — no SPDesktop needed.
 */
class GuidesPanel : public QWidget {
    Q_OBJECT

public:
    explicit GuidesPanel(QWidget* parent = nullptr);
    ~GuidesPanel() override;

    void setNamedview(SPNamedView* namedview);
    void update(SPNamedView* namedview);

private:
    void setupConnections();
    void addGuide(SPGuide* guide);
    void updatePlaceholder();
    void scrollToBottom();
    void watchDocument();

    std::unique_ptr<Ui::GuidesPanel> _ui;
    SPNamedView* _namedview = nullptr;
    SPDocument* _document = nullptr;
    OperationBlocker _update;
    sigc::scoped_connection _resourcesChanged;
    sigc::scoped_connection _commitConn;
};

} // namespace Linea::UI

#endif // LINEA_UI_GUIDES_PANEL_H
