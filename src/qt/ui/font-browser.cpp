// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontBrowser — reusable font browser widget.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2026 Authors
 */

#include "font-browser.h"

#include <QActionGroup>
#include <QCheckBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>
#include <set>

#include "font-list.h"
#include "preferences.h"
#include "ui_font-browser.h"
#include "util/font-tags.h"

namespace {
const char* sort_icon(Inkscape::FontOrder order) {
    switch (order) {
        case Inkscape::FontOrder::ByFamily:
            return ":/icons/sort-by-family";
        case Inkscape::FontOrder::ByWeight:
            return ":/icons/sort-by-weight";
        case Inkscape::FontOrder::ByWidth:
            return ":/icons/sort-by-width";
        default:
            return ":/icons/sort-alphabetically";
    }
}
} // namespace

namespace Linea::UI {

FontBrowser::FontBrowser(QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::FontBrowser>()) {
    ui->setupUi(this);
    ui->fontList->setHasFrame(true);
    ui->tagsScroll->setVisible(false);
    ui->infoLayout->setStretchFactor(ui->tagsScroll, 1);

    // sorting menu
    auto menu = new QMenu(this);
    auto group = new QActionGroup(this);
    group->setExclusive(true);

    const struct {
        const char* icon;
        QString label;
        Inkscape::FontOrder order;
    } sorting[] = {
        {":/icons/sort-by-family", tr("Group by family"), Inkscape::FontOrder::ByFamily},
        {":/icons/sort-by-weight", tr("Light to heavy"), Inkscape::FontOrder::ByWeight},
        {":/icons/sort-by-width", tr("Condensed to expanded"), Inkscape::FontOrder::ByWidth},
    };
    for (auto& entry : sorting) {
        auto action = menu->addAction(QIcon(entry.icon), entry.label);
        action->setCheckable(true);
        action->setData(static_cast<int>(entry.order));
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, order = entry.order] { setSortOrder(order); });
    }
    ui->sortButton->setMenu(menu);

    // restore sorting
    auto order = static_cast<Inkscape::FontOrder>(Inkscape::Preferences::get()->getIntLimited(
        (_prefsPath + "/font-order").toStdString(), static_cast<int>(Inkscape::FontOrder::ByFamily),
        static_cast<int>(Inkscape::FontOrder::_First), static_cast<int>(Inkscape::FontOrder::_Last)));
    setSortOrder(order);

    connect(ui->searchEntry, &QLineEdit::textChanged, ui->fontList, &FontList::filterFonts);
    // Enter in the search field hands focus to the list for keyboard navigation
    connect(ui->searchEntry, &QLineEdit::returnPressed, this, [this] {
        ui->fontList->setFocus();
    });

    // Esc asks to be dismissed no matter which child widget has focus;
    // the host decides what that means (hide popup, reject dialog, ...)
    auto esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, &FontBrowser::cancelled);

    // category menu: the checkable list sits inside a widget action so that
    // toggling entries does not close the menu; gives the button a real menu
    // and the drop-down arrow, same as the sort button
    _categoryList = createCategoryList();
    auto categoryMenu = new QMenu(this);
    auto categoryAction = new QWidgetAction(categoryMenu);
    categoryAction->setDefaultWidget(_categoryList);
    categoryMenu->addAction(categoryAction);
    ui->categoryButton->setMenu(categoryMenu);
    connect(categoryMenu, &QMenu::aboutToShow, this, &FontBrowser::syncCategoryChecks);

    connect(ui->fontList, &FontList::fontChanged, this, &FontBrowser::fontChanged);
    connect(ui->fontList, &FontList::fontSelected, this, &FontBrowser::fontSelected);
    connect(ui->fontList, &FontList::fontCountChanged, this, &FontBrowser::updateFontCount);

    // category selection is shared state; keep tags and the filter in sync
    _tagConnection = Inkscape::FontTags::get().get_signal_tag_changed().connect([this](const Inkscape::FontTag*, bool) {
        updateTags();
        applyCategoryFilter();
        if (_categoryList) {
            syncCategoryChecks();
        }
    });

    updateTags();
    updateFontCount(ui->fontList->fontCount(), ui->fontList->totalFontCount());
}

FontBrowser::~FontBrowser() = default;

void FontBrowser::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // put keyboard focus on the list so arrows/Enter work immediately
    ui->fontList->setFocus();
}

void FontBrowser::setPreferencesPath(const QString& path) {
    _prefsPath = path;
}

FontList* FontBrowser::fontList() const {
    return ui->fontList;
}

QString FontBrowser::currentFontspec() const {
    return ui->fontList->currentFontspec();
}

void FontBrowser::setCurrentFont(const QString& fontspec) {
    ui->fontList->setCurrentFont(fontspec);
}

void FontBrowser::setSortOrder(Inkscape::FontOrder order) {
    if (order == Inkscape::FontOrder::ByName) {
        order = Inkscape::FontOrder::ByFamily;
    }
    ui->fontList->setFontOrder(order);
    ui->sortButton->setIcon(QIcon(sort_icon(order)));
    Inkscape::Preferences::get()->setInt((_prefsPath + "/font-order").toStdString(), static_cast<int>(order));

    if (auto menu = ui->sortButton->menu()) {
        for (auto action : menu->actions()) {
            action->setChecked(action->data().toInt() == static_cast<int>(order));
        }
    }
}

QWidget* FontBrowser::createCategoryList() {
    auto list = new QWidget;
    auto layout = new QVBoxLayout(list);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(8);

    auto& tags = Inkscape::FontTags::get();
    for (auto& tag : tags.get_tags()) {
        auto btn = new QCheckBox(QString::fromUtf8(tag.display_name.raw()), list);
        btn->setProperty("tag", QString::fromStdString(tag.tag));
        connect(btn, &QCheckBox::toggled, list,
                [t = tag.tag](bool on) { Inkscape::FontTags::get().select_tag(t, on); });
        layout->addWidget(btn);
    }

    auto sep = new QFrame(list);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: palette(mid)");
    layout->addWidget(sep);

    auto reset = new QPushButton(QIcon(":/icons/edit-clear"), tr("Reset"), list);
    reset->setFlat(true);
    connect(reset, &QPushButton::clicked, list, [] { Inkscape::FontTags::get().deselect_all(); });
    layout->addWidget(reset);

    return list;
}

void FontBrowser::syncCategoryChecks() {
    // resync checkboxes; selection may have changed while the menu was closed
    auto& tags = Inkscape::FontTags::get();
    for (auto btn : _categoryList->findChildren<QCheckBox*>()) {
        auto tag = btn->property("tag").toString();
        if (tag.isEmpty()) continue;

        QSignalBlocker blocker(btn);
        btn->setChecked(tags.is_tag_selected(tag.toStdString()));
    }
}

void FontBrowser::updateTags() {
    auto& tags = Inkscape::FontTags::get();
    auto& selected = tags.get_selected_tags();

    while (auto item = ui->tagsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    for (auto& ft : selected) {
        auto pill = new QPushButton(QString::fromUtf8(ft.display_name.raw()) + "  ×", ui->tagsContainer);
        pill->setProperty("class", "tag-pill");
        pill->setToolTip(tr("Remove category filter"));
        connect(pill, &QPushButton::clicked, this,
                [tag = ft.tag] { Inkscape::FontTags::get().select_tag(tag, false); });
        ui->tagsLayout->addWidget(pill, 0, Qt::AlignVCenter);
        // a previously empty scroll area marks its content hidden; an explicit
        // show clears that flag or the layout treats the pill as an empty item
        pill->show();
    }
    ui->tagsContainer->show();

    // resizable is off so the container keeps its natural width and the row
    // scrolls horizontally; give it its content-derived size explicitly
    ui->tagsContainer->adjustSize();

    // pin the row to the pills' height; reserve space for the scrollbar
    // when it will be needed (classic scrollbars only, overlays take none)
    int height = ui->tagsContainer->sizeHint().height();
    auto hbar = ui->tagsScroll->horizontalScrollBar();
    if (ui->tagsContainer->width() > ui->tagsScroll->viewport()->width() && hbar &&
        !ui->tagsScroll->style()->styleHint(QStyle::SH_ScrollBar_Transient)) {
        height += hbar->sizeHint().height();
    }
    ui->tagsScroll->setFixedHeight(height);
    ui->tagsScroll->setVisible(!selected.empty());
}

void FontBrowser::applyCategoryFilter() {
    auto& tags = Inkscape::FontTags::get();
    auto& selected = tags.get_selected_tags();
    if (selected.empty()) {
        ui->fontList->setFontFilter({});
        return;
    }

    std::set<std::string> active;
    for (auto& ft : selected) {
        active.insert(ft.tag);
    }
    ui->fontList->setFontFilter([active = std::move(active)](const Inkscape::FontInfo& info) {
        if (!info.face) return false;
        auto faceTags = Inkscape::FontTags::get().get_font_tags(info.face);
        return std::any_of(active.begin(), active.end(), [&](const std::string& t) { return faceTags.count(t) > 0; });
    });
}

void FontBrowser::updateFontCount(int count, int total) {
    ui->countLabel->setText(count >= total ? tr("All fonts") : tr("%1 of %2 fonts").arg(count).arg(total));
}

} // namespace Linea::UI
