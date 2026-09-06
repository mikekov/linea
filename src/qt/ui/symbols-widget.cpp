// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Standalone Qt symbol gallery.
 */

#include "symbols-widget.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include <glibmm/i18n.h>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDataStream>
#include <QDrag>
#include <QFontMetrics>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTextOption>
#include <QVBoxLayout>

#include "document.h"
#include "object/sp-symbol.h"
#include "ui/clipboard.h"
#include "util/object-renderer.h"
#include "ui_symbols-widget.h"

namespace Linea::UI {
namespace {

constexpr auto SYMBOL_MIME_TYPE = "application/x-inkscape-symbol";
constexpr int SYMBOL_TILE_BASE_SIZE = 16;
constexpr int SYMBOL_TILE_MAX_INDEX = 50;
constexpr int DEFAULT_TILE_INDEX = 12;
constexpr int DEFAULT_TILE_SIZE = 32;
constexpr int DEFAULT_CACHE_LIMIT = 1000;

int symbol_tile_size(int index) {
    return static_cast<int>(std::round(SYMBOL_TILE_BASE_SIZE * std::pow(2.0, index / 12.0)));
}

class SymbolsModel final : public QAbstractListModel {
public:
    enum Roles {
        SymbolIdRole = Qt::UserRole + 1,
        UniqueKeyRole,
        SymbolPointerRole,
    };

    explicit SymbolsModel(QObject* parent = nullptr)
        : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(_symbols.size());
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        auto itemFlags = QAbstractListModel::flags(index);
        if (index.isValid() && index.row() < rowCount()) {
            itemFlags |= Qt::ItemIsDragEnabled;
        }
        return itemFlags;
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return {};
        }

        auto const& symbol = *_symbols[index.row()];
        switch (role) {
            case Qt::DisplayRole:
                return symbol.title;
            case Qt::ToolTipRole:
                return symbol.title;
            case SymbolIdRole:
                return symbol.id;
            case UniqueKeyRole:
                return symbol.uniqueKey;
            case SymbolPointerRole:
                return QVariant::fromValue(reinterpret_cast<quintptr>(&symbol));
            default:
                return {};
        }
    }

    void clear() {
        beginResetModel();
        _symbols.clear();
        endResetModel();
    }

    void append(std::vector<SymbolData> symbols) {
        if (symbols.empty()) {
            return;
        }

        auto first = static_cast<int>(_symbols.size());
        auto last = first + static_cast<int>(symbols.size()) - 1;
        std::vector<std::shared_ptr<SymbolData>> additions;
        additions.reserve(symbols.size());
        for (auto& symbol : symbols) {
            additions.push_back(std::make_shared<SymbolData>(std::move(symbol)));
        }

        beginInsertRows({}, first, last);
        _symbols.insert(_symbols.end(), std::make_move_iterator(additions.begin()),
                        std::make_move_iterator(additions.end()));
        endInsertRows();
    }

    const SymbolData* symbol(const QModelIndex& index) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return nullptr;
        }
        return _symbols[index.row()].get();
    }

private:
    std::vector<std::shared_ptr<SymbolData>> _symbols;
};

class SymbolsFilter final : public QSortFilterProxyModel {
public:
    explicit SymbolsFilter(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setSearchText(const QString& text) {
        beginFilterChange();
        _search = text;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (_search.trimmed().isEmpty()) {
            return true;
        }
        auto index = sourceModel()->index(row, 0, parent);
        auto text = index.data(SymbolsModel::SymbolIdRole).toString();
        text += QLatin1Char(' ');
        text += index.data(Qt::DisplayRole).toString();
        return text.contains(_search, Qt::CaseInsensitive);
    }

private:
    QString _search;
};

class SymbolDelegate final : public QStyledItemDelegate {
public:
    explicit SymbolDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void setTileSize(int size) {
        _tileSize = std::max(16, size);
        clearCache();
    }

    void setShowNames(bool show) {
        if (_showNames == show) {
            return;
        }
        _showNames = show;
    }

    void clearCache() const {
        _cache.clear();
        _order.clear();
    }

    static void configureLabelDocument(QTextDocument& document, const QFont& font,
                                       int width, const QString& text) {
        document.setDocumentMargin(0);
        document.setDefaultFont(font);
        document.setTextWidth(width);
        auto option = document.defaultTextOption();
        option.setAlignment(Qt::AlignHCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        document.setDefaultTextOption(option);
        document.setPlainText(text);
    }

    int labelHeight(const QStyleOptionViewItem& option, const QModelIndex& index) const {
        if (!_showNames) {
            return 0;
        }
        QTextDocument document;
        configureLabelDocument(document, option.font, std::max(1, option.rect.width() - 6),
                               index.data(Qt::DisplayRole).toString());
        return static_cast<int>(std::ceil(document.size().height()));
    }

    QImage image(const QModelIndex& index, qreal deviceScale) const {
        auto key = index.data(SymbolsModel::UniqueKeyRole).toString() + ':' +
                   QString::number(_tileSize) + ':' + QString::number(deviceScale);
        if (auto found = _cache.constFind(key); found != _cache.constEnd()) {
            _order.removeAll(key);
            _order.append(key);
            return *found;
        }

        auto pointer = index.data(SymbolsModel::SymbolPointerRole).value<quintptr>();
        auto symbol = reinterpret_cast<const SymbolData*>(pointer);
        if (!symbol || !symbol->object) {
            return {};
        }

        QImage rendered;
        try {
            Inkscape::object_renderer renderer;
            auto options = Inkscape::object_renderer::options().symbol_allow_upscale();
            rendered = renderer.render(static_cast<SPObject&>(*symbol->object), _tileSize, _tileSize,
                                       deviceScale, options);
            if (!rendered.isNull()) {
                rendered.setDevicePixelRatio(deviceScale);
            }
        } catch (...) {
            return {};
        }

        if (!rendered.isNull()) {
            _cache.insert(key, rendered);
            _order.append(key);
            while (_order.size() > DEFAULT_CACHE_LIMIT) {
                _cache.remove(_order.takeFirst());
            }
        }
        return rendered;
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->features |= QStyleOptionViewItem::WrapText;
        option->textElideMode = Qt::ElideNone;
    }

public:
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        auto width = _tileSize + 20;
        if (!_showNames) {
            return {width, _tileSize + 16};
        }

        auto text = index.data(Qt::DisplayRole).toString();
        QTextDocument document;
        configureLabelDocument(document, option.font, std::max(1, width - 6), text);
        auto textHeight = static_cast<int>(std::ceil(document.size().height()));
        return {width, _tileSize + 10 + std::max(16, textHeight)};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        if (!painter || !index.isValid()) {
            return;
        }

        painter->save();
        auto itemOption = option;
        itemOption.text.clear();
        if (itemOption.widget) {
            itemOption.widget->style()->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter,
                                                    itemOption.widget);
        }

        auto tileBottom = _showNames ? option.rect.bottom() - 6 - labelHeight(option, index) : option.rect.bottom() - 6;
        auto tile = QRect(option.rect.left() + 8, option.rect.top() + 6,
                          option.rect.width() - 16, tileBottom - option.rect.top() - 5);
        painter->setBrush(Qt::white);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(tile, 3, 3);

        auto rendered = image(index, option.widget ? option.widget->devicePixelRatioF() : 1.0);
        if (!rendered.isNull()) {
            auto imageSize = rendered.deviceIndependentSize().toSize();
            auto imageRect = QRect(QPoint(), imageSize);
            imageRect.moveCenter(tile.center());
            painter->drawImage(imageRect, rendered);
        }

        if (_showNames) {
            auto labelTop = tile.bottom() + 5;
            auto labelRect = QRect(option.rect.left() + 3, labelTop,
                                   option.rect.width() - 6, option.rect.bottom() - labelTop - 2);
            QTextDocument document;
            configureLabelDocument(document, option.font, labelRect.width(),
                                   index.data(Qt::DisplayRole).toString());
            painter->save();
            painter->setClipRect(labelRect);
            painter->translate(labelRect.topLeft());
            document.drawContents(painter, QRectF(QPointF(), labelRect.size()));
            painter->restore();
        }
        painter->restore();
    }

private:
    int _tileSize = DEFAULT_TILE_SIZE;
    bool _showNames = true;
    mutable QHash<QString, QImage> _cache;
    mutable QList<QString> _order;
};

class SymbolsView final : public QListView {
public:
    using PrepareDrag = std::function<bool(const QModelIndex&)>;
    using DragImage = std::function<QImage(const QModelIndex&, qreal)>;

    SymbolsView(PrepareDrag prepare, DragImage image, QWidget* parent = nullptr)
        : QListView(parent)
        , _prepare(std::move(prepare))
        , _image(std::move(image)) {}

    void prepareCurrent() {
        if (_prepare) {
            _prepare(currentIndex());
        }
    }

protected:
    void startDrag(Qt::DropActions supportedActions) override {
        auto index = currentIndex();
        if (!index.isValid() || !_prepare(index)) {
            return;
        }

        auto mime = new QMimeData;
        QByteArray payload;
        QDataStream stream(&payload, QIODevice::WriteOnly);
        stream << index.data(SymbolsModel::SymbolIdRole).toString()
               << index.data(SymbolsModel::UniqueKeyRole).toString();
        mime->setData(SYMBOL_MIME_TYPE, payload);

        auto drag = new QDrag(this);
        drag->setMimeData(mime);
        if (_image) {
            auto image = _image(index, devicePixelRatioF());
            if (!image.isNull()) {
                drag->setPixmap(QPixmap::fromImage(image));
                drag->setHotSpot(drag->pixmap().rect().center());
            }
        }
        drag->exec(supportedActions & Qt::CopyAction ? Qt::CopyAction : Qt::IgnoreAction);
    }

private:
    PrepareDrag _prepare;
    DragImage _image;
};

} // namespace

SymbolsWidget::SymbolsWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::SymbolsWidget>()) {
    _ui->setupUi(this);
    auto collection = _ui->collection;
    auto search = _ui->search;
    auto info = _ui->info;
    auto tileSize = _ui->tileSize;

    auto model = new SymbolsModel(this);
    auto filter = new SymbolsFilter(this);
    filter->setSourceModel(model);
    auto delegate = new SymbolDelegate(this);
    delegate->setTileSize(symbol_tile_size(DEFAULT_TILE_INDEX));
    auto view = new SymbolsView(
        [model](const QModelIndex& proxyIndex) {
            auto sourceIndex = static_cast<const SymbolsFilter*>(proxyIndex.model())->mapToSource(proxyIndex);
            auto symbol = model->symbol(sourceIndex);
            if (!symbol || !symbol->object || !symbol->document) {
                return false;
            }
            auto bbox = Geom::Rect(-0.5 * symbol->dimensions, 0.5 * symbol->dimensions);
            auto sourceName = symbol->document->getDocumentFilename();
            auto style = symbol->document->getReprRoot()->attribute("style");
            Inkscape::UI::ClipboardManager::get()->copySymbol(
                symbol->object->getRepr(), style, symbol->document, sourceName, bbox, false);
            return true;
        },
        [delegate](const QModelIndex& index, qreal scale) {
            return delegate->image(index, scale);
        },
        this);
    view->setModel(filter);
    view->setItemDelegate(delegate);
    view->setObjectName(QStringLiteral("symbols-view"));
    view->setViewMode(QListView::IconMode);
    view->setFlow(QListView::LeftToRight);
    view->setWordWrap(true);
    view->setTextElideMode(Qt::ElideNone);
    view->setWrapping(true);
    view->setResizeMode(QListView::Adjust);
    view->setLayoutMode(QListView::Batched);
    view->setBatchSize(24);
    view->setMovement(QListView::Static);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setDragEnabled(true);
    _ui->mainLayout->replaceWidget(_ui->view, view);
    _ui->view->deleteLater();

    auto updateInfo = [model, filter, info]() {
        auto total = model->rowCount();
        auto visible = filter->rowCount();
        if (total == 0) {
            info->clear();
        } else if (total == visible) {
            info->setText(QString::fromUtf8(_("Symbols: %1")).arg(total));
        } else {
            info->setText(QString::fromUtf8(_("Symbols: %1 / %2")).arg(visible).arg(total));
        }
    };
    connect(collection, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (_collectionChanged) {
            _collectionChanged(index);
        }
    });
    connect(search, &QLineEdit::textChanged, this, [filter, updateInfo](const QString& text) {
        filter->setSearchText(text);
        updateInfo();
    });
    connect(tileSize, &QSlider::valueChanged, this, [delegate, view](int value) {
        delegate->setTileSize(symbol_tile_size(value));
        view->doItemsLayout();
        view->viewport()->update();
    });
    connect(view, &QListView::activated, this, [this, view](const QModelIndex&) {
        view->prepareCurrent();
        Q_EMIT symbolActivated();
    });

    _clearSymbols = [model, updateInfo]() {
        model->clear();
        updateInfo();
    };
    _setSymbols = [model, updateInfo](std::vector<SymbolData> symbols) {
        model->clear();
        model->append(std::move(symbols));
        updateInfo();
    };
    _appendSymbols = [model, updateInfo](std::vector<SymbolData> symbols) {
        model->append(std::move(symbols));
        updateInfo();
    };
}

void SymbolsWidget::clearSymbols() {
    _clearSymbols();
}

void SymbolsWidget::setSymbols(std::vector<SymbolData> symbols) {
    _setSymbols(std::move(symbols));
}

void SymbolsWidget::appendSymbols(std::vector<SymbolData> symbols) {
    _appendSymbols(std::move(symbols));
}

void SymbolsWidget::setCollectionNames(const QStringList& names) {
    auto collection = findChild<QComboBox*>();
    if (!collection) {
        return;
    }
    collection->clear();
    collection->addItems(names);
}

void SymbolsWidget::setCollectionChanged(std::function<void(int)> callback) {
    _collectionChanged = std::move(callback);
}

SymbolsWidget::~SymbolsWidget() = default;

} // namespace Linea::UI
