// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Stock symbol source for the generic Qt symbols widget. */

#ifndef LINEA_UI_STOCK_SYMBOL_SOURCE_H
#define LINEA_UI_STOCK_SYMBOL_SOURCE_H

#include <QObject>

#include <memory>
#include <vector>

#include "symbols-widget.h"

class SPDocument;

namespace Linea::UI {

class StockSymbolsSource final : public QObject {
    Q_OBJECT

public:
    explicit StockSymbolsSource(SymbolsWidget* widget, QObject* parent = nullptr);
    ~StockSymbolsSource() override;

    int setCount() const;
    QString setTitle(int index) const;
    void loadSet(int index);
    void loadAll();

private:
    struct Set;

    void load(Set& set);
    void loadNext(int generation, std::shared_ptr<int> next);

    SymbolsWidget* _widget;
    std::vector<std::unique_ptr<Set>> _sets;
    int _generation = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_STOCK_SYMBOL_SOURCE_H
