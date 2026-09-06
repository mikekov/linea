// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Empty welcome page widget implementation.
 */

#include "welcome-page.h"

#include <QAction>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <giomm.h>

#include "actions/action-registry.h"
#include "ui/desktop/document-templates-menu.h"
#include "io/recent-files.h"
#include "linea-application.h"
#include "preferences.h"
#include "qt/ui/eliding-label.h"
#include "qt/ui/icon-widget.h"
#include "ui_welcome-page.h"

namespace Linea::UI {

WelcomePage::WelcomePage(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::WelcomePage>()) {
    _ui->setupUi(this);
    auto searchAction = new QAction(QIcon(":/icons/searching"), {}, _ui->recentFilesSearch);
    _ui->recentFilesSearch->addAction(searchAction, QLineEdit::LeadingPosition);
    _ui->recentFilesSearch->installEventFilter(this);
    _ui->recentFilesSearch->setFixedWidth(40);
    _searchAnimation = new QPropertyAnimation(_ui->recentFilesSearch, "maximumWidth", this);
    _searchAnimation->setDuration(180);
    _searchAnimation->setEasingCurve(QEasingCurve::OutCubic);
    setSearchExpanded(false);
    _ui->recentFilesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _ui->recentFilesList->setMouseTracking(true);
    _ui->recentFilesList->viewport()->setMouseTracking(true);

    for (auto list : {_ui->recentFilesList, _ui->recoverFilesList}) {
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setMouseTracking(true);
        list->viewport()->setMouseTracking(true);
        connect(list, &QListWidget::itemClicked, this, &WelcomePage::openRecentFile);
        connect(list, &QListWidget::itemActivated, this, &WelcomePage::openRecentFile);
    }

    _ui->templatesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _ui->templatesList->setMouseTracking(true);
    _ui->templatesList->viewport()->setMouseTracking(true);
    connect(_ui->templatesList, &QListWidget::itemClicked, this, &WelcomePage::openTemplate);
    connect(_ui->templatesList, &QListWidget::itemActivated, this, &WelcomePage::openTemplate);
    setTabOrder(_ui->recentFilesList, _ui->recentFilesSearch);
    connect(_ui->recentFilesSearch, &QLineEdit::textChanged, this, [this] {
        rebuildRecentFiles(_ui->recentFilesList, false);
    });
}

WelcomePage::~WelcomePage() = default;

void WelcomePage::setSearchExpanded(bool expanded) {
    if (!_ui->recentFilesSearch || !_searchAnimation) return;

    auto width = expanded ? 140 : 40;
    if (_ui->recentFilesSearch->width() == width) return;

    _searchAnimation->stop();
    _ui->recentFilesSearch->setMinimumWidth(40);
    _searchAnimation->setStartValue(_ui->recentFilesSearch->width());
    _searchAnimation->setEndValue(width);
    _searchAnimation->start();
}

bool WelcomePage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _ui->recentFilesSearch) {
        if (event->type() == QEvent::FocusIn) {
            setSearchExpanded(true);
        } else if (event->type() == QEvent::FocusOut && _ui->recentFilesSearch->text().isEmpty()) {
            setSearchExpanded(false);
        }
    }
    return QWidget::eventFilter(watched, event);
}

void WelcomePage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    rebuildRecentFiles(_ui->recentFilesList, false);
    rebuildRecentFiles(_ui->recoverFilesList, true);
    rebuildTemplates();
}

void WelcomePage::openRecentFile(QListWidgetItem* item) {
    if (!item) return;

    auto path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    auto file = Gio::File::create_for_path(path.toStdString());
    LINEA_APP.openDocument(file);
}

void WelcomePage::rebuildRecentFiles(QListWidget* list, bool autosave) {
    if (!list) return;

    list->clear();

    int max_files = Inkscape::Preferences::get()->getInt("/options/maxrecentdocuments/value", 20);
    if (max_files <= 0) return;

    if (max_files > 20) {
        max_files = 20; // TODO: find max
    }

    auto recent = Linea::IO::getRecentFiles(max_files, autosave);
    auto shortened = Linea::IO::getShortenedPathMap(recent);
    auto query = !autosave && _ui->recentFilesSearch ? _ui->recentFilesSearch->text().trimmed() : QString{};
    for (auto& file : recent) {
        if (!query.isEmpty()) {
            auto path = QString::fromStdString(file.path);
            auto name = QString::fromStdString(file.display_name);
            if (!path.contains(query, Qt::CaseInsensitive) && !name.contains(query, Qt::CaseInsensitive)) {
                continue;
            }
        }

        auto listItem = new QListWidgetItem(list);
        listItem->setData(Qt::UserRole, QString::fromStdString(file.path));

        auto row = new QFrame(list);
        row->setProperty("class", "custom-tool-item");
        row->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->setCursor(Qt::PointingHandCursor);
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto layout = new QVBoxLayout(row);
        layout->setContentsMargins(8, 6, 12, 6);
        layout->setSpacing(0);

        auto title = QString::fromStdString(shortened[file.path]);
        if (title.isEmpty()) title = QString::fromStdString(file.display_name);
        auto titleLabel = new ElidingLabel(row);
        titleLabel->setProperty("class", "menu-item-label");
        titleLabel->setOverflowMode(ElidingLabel::OverflowMode::Elide);
        titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        titleLabel->setFullText(title);
        layout->addWidget(titleLabel);

        auto description = QString::fromStdString(file.path);
        if (!description.isEmpty()) {
            auto descriptionLabel = new ElidingLabel(row);
            descriptionLabel->setProperty("class", "menu-item-description");
            descriptionLabel->setOverflowMode(ElidingLabel::OverflowMode::Fade);
            descriptionLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            descriptionLabel->setFullText(description);
            layout->addWidget(descriptionLabel);
        }

        listItem->setSizeHint(row->sizeHint());
        list->setItemWidget(listItem, row);
    }
}

void WelcomePage::openTemplate(QListWidgetItem* item) {
    if (!item) return;

    auto actionId = item->data(Qt::UserRole).toString();
    if (actionId.isEmpty()) return;

    auto action = ActionRegistry::get().action(actionId.toStdString());
    if (action) action->trigger();
}

void WelcomePage::rebuildTemplates() {
    auto list = _ui->templatesList;
    if (!list) return;

    list->clear();

    for (const auto& tmpl : newDocumentFromTemplateMenu()) {
        auto listItem = new QListWidgetItem(list);
        listItem->setData(Qt::UserRole, QString::fromStdString(tmpl.action));

        auto row = new QFrame(list);
        row->setProperty("class", "custom-tool-item");
        row->setAttribute(Qt::WA_TransparentForMouseEvents);
        row->setCursor(Qt::PointingHandCursor);
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(8, 6, 12, 6);
        hbox->setSpacing(8);

        if (!tmpl.icon.isNull()) {
            auto icon = new IconWidget(row);
            icon->setIcon(tmpl.icon);
            icon->setIconSize(tmpl.iconSize);
            hbox->addWidget(icon);
        }

        auto text = new QVBoxLayout();
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(0);

        auto titleLabel = new QLabel(tmpl.title, row);
        titleLabel->setProperty("class", "menu-item-label");
        text->addWidget(titleLabel);

        if (!tmpl.description.isEmpty()) {
            auto descLabel = new QLabel(tmpl.description, row);
            descLabel->setProperty("class", "menu-item-description");
            text->addWidget(descLabel);
        }

        hbox->addLayout(text);
        hbox->addStretch();

        listItem->setSizeHint(row->sizeHint());
        list->setItemWidget(listItem, row);
    }
}

} // namespace Linea::UI
