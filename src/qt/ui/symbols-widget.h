// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Standalone Qt symbol gallery.
 */

#ifndef LINEA_UI_SYMBOLS_WIDGET_H
#define LINEA_UI_SYMBOLS_WIDGET_H

#include <QWidget>
#include <QString>
#include <QStringList>

#include <2geom/point.h>

#include <functional>
#include <memory>
#include <vector>

class SPDocument;
class SPSymbol;

namespace Ui {
class SymbolsWidget;
}

namespace Geom { class Point; }

namespace Linea::UI {

struct SymbolData {
    QString id;
    QString title;
    QString uniqueKey;
    Geom::Point dimensions;
    SPDocument* document = nullptr;
    SPSymbol* object = nullptr;
};

/**
 * Virtual symbol gallery. The widget is source-agnostic: callers provide
 * symbol records and retain ownership of their source documents.
 */
class SymbolsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit SymbolsWidget(QWidget* parent = nullptr);
    ~SymbolsWidget() override;

    void clearSymbols();
    void setSymbols(std::vector<SymbolData> symbols);
    void appendSymbols(std::vector<SymbolData> symbols);
    void setCollectionNames(const QStringList& names);
    void setCollectionChanged(std::function<void(int)> callback);

Q_SIGNALS:
    void symbolActivated();

private:
    std::unique_ptr<Ui::SymbolsWidget> _ui;
    std::function<void()> _clearSymbols;
    std::function<void(std::vector<SymbolData>)> _setSymbols;
    std::function<void(int)> _collectionChanged;
    std::function<void(std::vector<SymbolData>)> _appendSymbols;
};

} // namespace Linea::UI

#endif // LINEA_UI_SYMBOLS_WIDGET_H
