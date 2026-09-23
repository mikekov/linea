// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PointWidget — editable SVG path and polyline point data.
 */

#ifndef LINEA_UI_POINT_WIDGET_H
#define LINEA_UI_POINT_WIDGET_H

#include <QPointer>
#include <QWidget>
#include <memory>
#include <sigc++/connection.h>

#include "syntax.h"

class QPlainTextEdit;
class SPObject;

namespace Ui {
class PointWidget;
}

namespace Linea::UI {

/**
 * A syntax-colored editor for SVG path data and polyline/polygon
 * points. The edited value is committed explicitly with the commit button or
 * Shift+Enter.
 */
class PointWidget : public QWidget {
    Q_OBJECT

public:
    explicit PointWidget(QWidget* parent = nullptr);
    ~PointWidget() override;

    /// Set the path, polyline, or polygon being edited. Pass nullptr to clear.
    void setObject(SPObject* object);
    SPObject* object() const { return _object; }

    /// Refresh the editor from the current object's SVG attribute.
    void updateUi();

Q_SIGNALS:
    void committed();

private:
    void onCommit();
    void roundNumbers();
    void setPrecision(int precision);
    bool eventFilter(QObject* watched, QEvent* event) override;
    void updateEditor();
    void commitValue();
    void setSyntaxMode(Syntax::SyntaxMode mode);
    QString attributeName() const;
    QString undoKey() const;
    QString undoLabel() const;
    bool isSupportedObject(SPObject* object) const;
    int nodeCount() const;

    std::unique_ptr<Ui::PointWidget> _ui;
    std::unique_ptr<Syntax::TextEditView> _editor;
    QPointer<QPlainTextEdit> _editorWidget;
    SPObject* _object = nullptr;
    sigc::connection _modified;
    sigc::connection _release;
    Syntax::SyntaxMode _syntaxMode = Syntax::SyntaxMode::SvgPathData;
    int _precision = 2;
    bool _pathOriginal = false;
    bool _updating = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_POINT_WIDGET_H
