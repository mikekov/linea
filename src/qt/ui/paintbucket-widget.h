// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Paint bucket tool options widget. */
#ifndef LINEA_UI_PAINTBUCKET_WIDGET_H
#define LINEA_UI_PAINTBUCKET_WIDGET_H

#include <QWidget>
#include <memory>

class SPDesktop;

namespace Linea::UI {
class UnitTracker;
}

namespace Ui {
class PaintbucketWidget;
}

namespace Linea::UI {
class PaintbucketWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaintbucketWidget(QWidget* parent = nullptr);
    ~PaintbucketWidget() override;
    void setDesktop(SPDesktop* desktop);

private:
    void resetDefaults();
    std::unique_ptr<Ui::PaintbucketWidget> _ui;
    std::unique_ptr<UnitTracker> _tracker;
    SPDesktop* _desktop = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAINTBUCKET_WIDGET_H
