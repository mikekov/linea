// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Color picker button with ColorNotebook popup
 */

#ifndef LINEA_UI_COLOR_PICKER_H
#define LINEA_UI_COLOR_PICKER_H

#include <QPushButton>
#include <QString>
#include <QWidget>
#include <memory>
#include <sigc++/signal.h>

#include "colors/color.h"
#include "ui/operation-blocker.h"
#include "popup-menu.h"

class SPDesktop;

namespace Inkscape::Colors {
class ColorSet;
}

namespace Linea::UI {

class ColorPreview;
class ColorNotebook;

class ColorPicker : public QPushButton {
    Q_OBJECT

public:
    explicit ColorPicker(QWidget* parent = nullptr);

    ColorPicker(SPDesktop* desktop, QString title, QString tip, const Inkscape::Colors::Color& initial,
                bool undo, bool use_transparency = true, QWidget* parent = nullptr);
    ~ColorPicker() override;

    void setColor(const Inkscape::Colors::Color& color);
    void open();
    void close();
    void setTitle(QString title);
    void setIcon(const QString& icon_name);
    void setDesktop(SPDesktop* desktop);
    void setUndo(bool undo);
    void setUseTransparency(bool use_transparency);

    Inkscape::Colors::Color getCurrentColor() const;

    sigc::connection connectChanged(sigc::slot<void(const Inkscape::Colors::Color&)> slot);
    sigc::signal<void(void)> signalOpenPopup();

Q_SIGNALS:
    void colorChanged(Inkscape::Colors::Color color);

private:
    void init();
    void onSelectedColorChanged();
    void onColorChanged(const Inkscape::Colors::Color& color);
    void constructPopup();
    void setPreview(std::uint32_t rgba);

    SPDesktop* _desktop = nullptr;
    ColorPreview* _preview = nullptr;
    QString _title;
    sigc::signal<void(const Inkscape::Colors::Color&)> _changed_signal;
    bool _undo = false;
    OperationBlocker _update;
    bool _use_transparency = true;
    std::shared_ptr<Inkscape::Colors::ColorSet> _colors;
    PopupMenu* _popup = nullptr;
    ColorNotebook* _color_selector = nullptr;
    sigc::signal<void(void)> _signal_open;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_PICKER_H
