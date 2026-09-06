// SPDX-License-Identifier: GPL-2.0-or-later

#include "filter-gallery.h"

#include <QLineEdit>
#include <QStandardItemModel>
#include <QSlider>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <glibmm/i18n.h>
#include <giomm/file.h>

#include "document.h"
#include "extension/db.h"
#include "extension/effect.h"
#include "gallery-filter-proxy.h"
#include "gallery-widget.h"
#include "io/file.h"
#include "io/resource.h"
#include "object/sp-item.h"
#include "ui/svg-renderer.h"
#include "ui/util.h"
#include "util/cast.h"
#include "ui_filter-gallery.h"

namespace Linea::UI {

namespace {

QSize thumbnail_size(int index) {
    constexpr int minSize = 50;
    auto factor = std::pow(2.0, 1.0 / 6.0);
    auto size = qRound(std::pow(factor, index) * minSize);
    return {size, size};
}

QImage render_preview(const QModelIndex& index, const QSize& size, qreal devicePixelRatio, SPDocument* document) {
    auto id = index.data(Qt::UserRole).toString();
    auto ext = Inkscape::Extension::db.get(id.toUtf8().constData());
    auto effect = dynamic_cast<Inkscape::Extension::Effect*>(ext);
    auto pixelRatio = std::max<qreal>(1.0, devicePixelRatio);
    auto fallback = [&]() {
        auto image = QImage(qRound(size.width() * pixelRatio), qRound(size.height() * pixelRatio),
                            QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        image.setDevicePixelRatio(pixelRatio);
        return image;
    };

    if (!effect || !document) return fallback();

    try {
        Inkscape::svg_renderer renderer(*document);
        renderer.set_style("*", "enable-background", "new");
        if (auto item = cast<SPItem>(document->getObjectById("test-object"))) {
            effect->apply_filter(item);
        }

        auto width = renderer.get_width_px();
        auto height = renderer.get_height_px();
        if (width > 0 && height > 0 && size.width() > 0 && size.height() > 0) {
            auto scale = std::max(width / size.width(), height / size.height());
            renderer.set_scale(1.0 / scale);
        }

        auto image = renderer.render_qimage(pixelRatio);
        if (!image.isNull()) return image;
    }
    catch (...) {
        return fallback();
    }

    return fallback();
}

} // namespace

FilterGallery::FilterGallery(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::FilterGallery>()) {
    _ui->setupUi(this);

    // example object to apply filters to
    auto example = Inkscape::IO::Resource::get_path_string(Inkscape::IO::Resource::SYSTEM, Inkscape::IO::Resource::UIS, "resources", "filter-test.svg");
    _document = ink_file_open(Gio::File::create_for_path(example)).first;

    auto effects = Inkscape::Extension::db.get_effect_list();
    std::vector<Inkscape::Extension::Effect*> filtered;
    std::copy_if(effects.begin(), effects.end(), std::back_inserter(filtered), [](auto effect) {
        return effect && !effect->hidden_from_menu() && effect->is_filter_effect() && !effect->deactivated();
    });

    std::sort(filtered.begin(), filtered.end(), [](auto lhs, auto rhs) {
        auto lhsName = cleanGalleryName(lhs->get_name());
        auto rhsName = cleanGalleryName(rhs->get_name());
        return lhsName.localeAwareCompare(rhsName) < 0;
    });

    _model = new QStandardItemModel(this);
    for (auto effect : filtered) {
        auto item = new QStandardItem(cleanGalleryName(effect->get_name()));
        auto menu = effect->get_menu_list();
        QString access;
        QString category;
        for (const auto& part : menu) {
            auto translated = QString::fromUtf8(part.c_str());
            if (category.isEmpty()) category = translated;
            if (!access.isEmpty()) access += QStringLiteral(" ▸ ");
            access += translated;
        }
        if (!access.isEmpty()) access += QStringLiteral(" ▸ ");
        access += item->text();

        auto description = effect->get_menu_tip();

        auto id = effect->get_sanitized_id();
        item->setData(QString::fromStdString(id), Qt::UserRole);
        item->setData(access, AccessRole);
        item->setData(category, CategoryRole);
        item->setData(QString::fromUtf8(description.c_str()), DescriptionRole);
        item->setData(QString::fromStdString(id), PreviewKeyRole);
        item->setData(effect->takes_input(), RequiresParametersRole);
        _model->appendRow(item);
    }

    _categories = new QStandardItemModel(this);
    auto all = new QStandardItem(tr("All Filters"));
    all->setData(QString(), CategoryRole);
    _categories->appendRow(all);

    QSet<QString> categoryNames;
    for (int row = 0; row < _model->rowCount(); ++row) {
        auto category = _model->index(row, 0).data(CategoryRole).toString();
        if (!category.isEmpty()) {
            categoryNames.insert(category);
        }
    }
    auto sortedCategories = categoryNames.values();
    std::sort(sortedCategories.begin(), sortedCategories.end());
    for (const auto& category : sortedCategories) {
        auto item = new QStandardItem(category);
        item->setData(category, CategoryRole);
        _categories->appendRow(item);
    }

    _filter = new GalleryFilterProxyModel(this);
    _filter->setSourceModel(_model);
    _ui->gallery->setModel(_filter);
    connect(_ui->gallery, &GalleryWidget::categoryChanged, this, [this](const QModelIndex& index) {
        auto proxy = static_cast<GalleryFilterProxyModel*>(_filter);
        proxy->setCategory(index.data(CategoryRole).toString());
    });
    _ui->gallery->setCategoryModel(_categories);
    _ui->gallery->setPreviewProvider([this](auto index, auto size, auto scale){ return render_preview(index, size, scale, _document.get()); });
    _ui->gallery->setShowDescriptions(false);
    auto applyThumbnailSize = [this](int value) {
        auto size = thumbnail_size(value);
        _ui->gallery->setThumbnailSize(size);
        _ui->gallery->setCardWidth(size.width() + 32);
    };
    _ui->thumbSize->setValue(6);
    applyThumbnailSize(_ui->thumbSize->value());
    connect(_ui->thumbSize, &QSlider::valueChanged, this, applyThumbnailSize);
    _ui->title->setText(tr("Filters"));
    _ui->gallery->setHeaderVisible(false);
    _ui->gallery->setFooterButtonText(tr("Apply"));
    _ui->gallery->setFooterButtonSuffix(tr("..."));

    connect(_ui->search, &QLineEdit::textChanged, this, [this](const QString& text) {
        auto proxy = static_cast<GalleryFilterProxyModel*>(_filter);
        proxy->setSearchText(text);
    });
    connect(_ui->gallery, &GalleryWidget::itemActivated, this, &FilterGallery::activate);
    connect(_ui->gallery, &GalleryWidget::actionRequested, this, &FilterGallery::requestAction);
}

FilterGallery::~FilterGallery() = default;

void FilterGallery::activate(const QModelIndex& index) {
    if (!index.isValid()) return;

    auto id = index.data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    Q_EMIT itemActivated(id);
}

void FilterGallery::requestAction(const QModelIndex& index) {
    if (!index.isValid()) return;

    auto id = index.data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    Q_EMIT actionRequested(id);
}

} // namespace Linea::UI
