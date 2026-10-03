// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontBrowser — reusable font browser widget with sorting, searching
 * and font category filtering, built around FontList.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 */

#ifndef LINEA_UI_FONT_BROWSER_H
#define LINEA_UI_FONT_BROWSER_H

#include <QWidget>
#include <memory>
#include <sigc++/scoped_connection.h>

#include "util/font-discovery.h"

namespace Ui {
class FontBrowser;
}

namespace Linea::UI {

class FontList;

/**
 * Reusable font browser panel.
 *
 * Layout: header row (sort menu, category filter button, search field),
 * a filter bar showing selected font categories as removable tags plus the
 * number of displayed fonts, and a FontList body.
 *
 * Categories (monospace, variable, serif, ...) come from the shared
 * Inkscape::FontTags registry; selecting any of them filters the list to
 * fonts carrying at least one of the selected tags.
 */
class FontBrowser : public QWidget {
    Q_OBJECT

public:
    explicit FontBrowser(QWidget* parent = nullptr);
    ~FontBrowser() override;

    // path under which widget settings are stored in preferences
    // (font sort order); default is "/options/fontbrowser"
    void setPreferencesPath(const QString& path);

    // direct access to the embedded font list (preview size, sample text, ...)
    FontList* fontList() const;

    QString currentFontspec() const;
    void setCurrentFont(const QString& fontspec);

Q_SIGNALS:
    // emitted when the highlighted font changes
    void fontChanged(const QString& fontspec);
    // emitted when the user confirms a font (double-click or Enter)
    void fontSelected(const QString& fontspec);
    // emitted on Esc — the host can dismiss/close the browser
    void cancelled();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setSortOrder(Inkscape::FontOrder order);
    void updateTags();
    void updateFontCount(int count, int total);
    void applyCategoryFilter();
    void syncCategoryChecks();
    QWidget* createCategoryList();

    std::unique_ptr<Ui::FontBrowser> ui;
    QWidget* _categoryList = nullptr;
    QString _prefsPath = "/options/fontbrowser";
    sigc::scoped_connection _tagConnection;
};

} // namespace Linea::UI

#endif // LINEA_UI_FONT_BROWSER_H
