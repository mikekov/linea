// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Empty welcome page widget.
 */

#ifndef LINEA_UI_WELCOME_PAGE_H
#define LINEA_UI_WELCOME_PAGE_H

#include <QShowEvent>
#include <QWidget>
#include <memory>

class QEvent;
class QListWidget;
class QPropertyAnimation;
class QListWidgetItem;
class QObject;

namespace Ui {
class WelcomePage;
}

namespace Linea::UI {

class WelcomePage : public QWidget {
    Q_OBJECT

public:
    explicit WelcomePage(QWidget* parent = nullptr);
    ~WelcomePage() override;

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setSearchExpanded(bool expanded);
    void rebuildRecentFiles(QListWidget* list, bool autosave);
    void rebuildTemplates();
    void openRecentFile(QListWidgetItem* item);
    void openTemplate(QListWidgetItem* item);

    std::unique_ptr<Ui::WelcomePage> _ui;
    QPropertyAnimation* _searchAnimation = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_WELCOME_PAGE_H
