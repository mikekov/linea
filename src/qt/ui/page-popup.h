// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Page popup — margins and bleed editor for the selected page,
 * opened from PageWidget inside a PopupMenu.
 */

#ifndef LINEA_UI_PAGE_POPUP_H
#define LINEA_UI_PAGE_POPUP_H

#include <QWidget>

#include <memory>

namespace Ui {
class PagePopup;
}

namespace Linea::Props {
class Binder;
}

namespace Linea::UI {

class PagePopup : public QWidget {
    Q_OBJECT

public:
    explicit PagePopup(QWidget* parent = nullptr);
    ~PagePopup() override;

    void bind(Props::Binder& binder);

private:
    std::unique_ptr<Ui::PagePopup> _ui;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAGE_POPUP_H
