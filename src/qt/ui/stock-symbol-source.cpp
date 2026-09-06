// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Stock symbol source for the generic Qt symbols widget. */

#include "stock-symbol-source.h"

#include <algorithm>
#include <fstream>
#include <regex>

#include <glibmm/i18n.h>
#include <QTimer>

#include "document.h"
#include "io/resource.h"
#include "object/sp-root.h"
#include "object/sp-symbol.h"
#include "util/cast.h"

namespace Linea::UI {
namespace {

struct StockSetData {
    std::string filename;
    QString title;
    std::unique_ptr<SPDocument> document;
};

QString read_title(const std::string& filename) {
    static const std::regex title_regex(".*?<title.*?>(.*?)<(/| /)");
    std::ifstream input(filename);
    std::string line;
    while (std::getline(input, line)) {
        std::smatch match;
        if (std::regex_search(line, match, title_regex) && match.size() > 1 && !match[1].str().empty()) {
            return QString::fromUtf8(g_dpgettext2(nullptr, "Symbol", match[1].str().c_str()));
        }
        if (line.find("<defs") != std::string::npos) {
            break;
        }
    }

    auto slash = filename.find_last_of("/\\");
    auto title = slash == std::string::npos ? filename : filename.substr(slash + 1);
    auto dot = title.rfind('.');
    if (dot != std::string::npos) {
        title.erase(dot);
    }
    return QString::fromUtf8(title.empty() ? _("Unnamed Symbols") : title.c_str());
}

void collect_symbols(SPObject* object, std::vector<SPSymbol*>& result) {
    if (!object) {
        return;
    }
    if (auto symbol = cast<SPSymbol>(object)) {
        result.push_back(symbol);
    }
    for (auto& child : object->children) {
        collect_symbols(&child, result);
    }
}

std::vector<SymbolData> make_symbols(SPDocument* document) {
    std::vector<SymbolData> result;
    if (!document) {
        return result;
    }

    std::vector<SPSymbol*> symbols;
    collect_symbols(document->getRoot(), symbols);
    result.reserve(symbols.size());
    for (auto symbol : symbols) {
        if (!symbol) {
            continue;
        }
        auto id = symbol->getId();
        if (!id || !*id) {
            continue;
        }

        auto title = symbol->title();
        auto shortTitle = title ? QString::fromUtf8(g_dpgettext2(nullptr, "Symbol", title))
                                 : QString::fromUtf8(id);
        g_free(title);

        auto dimensions = Geom::Point{64, 64};
        if (auto bounds = symbol->documentVisualBounds()) {
            dimensions = bounds->dimensions();
        }

        auto filename = document->getDocumentFilename();
        SymbolData data;
        data.id = QString::fromUtf8(id);
        data.title = shortTitle;
        data.uniqueKey = QString::fromStdString((filename ? filename : "") + std::string{"\n"} + id);
        data.dimensions = dimensions;
        data.document = document;
        data.object = symbol;
        result.push_back(std::move(data));
    }
    return result;
}

} // namespace

struct StockSymbolsSource::Set : StockSetData {};

StockSymbolsSource::StockSymbolsSource(SymbolsWidget* widget, QObject* parent)
    : QObject(parent)
    , _widget(widget) {
    if (!_widget) {
        return;
    }

    for (auto const& filename : Inkscape::IO::Resource::get_filenames(
             Inkscape::IO::Resource::SYMBOLS, {".svg"})) {
        auto set = std::make_unique<Set>();
        set->filename = filename;
        set->title = read_title(filename);
        _sets.push_back(std::move(set));
    }
    std::sort(_sets.begin(), _sets.end(), [](auto const& lhs, auto const& rhs) {
        return lhs->title.localeAwareCompare(rhs->title) < 0;
    });

    QStringList names{QString::fromUtf8(_("All symbol sets"))};
    for (auto const& set : _sets) {
        names.append(set->title);
    }
    _widget->setCollectionNames(names);
    _widget->setCollectionChanged([this](int index) {
        if (index == 0) {
            loadAll();
        } else {
            loadSet(index - 1);
        }
    });
    loadAll();
}

StockSymbolsSource::~StockSymbolsSource() = default;

int StockSymbolsSource::setCount() const {
    return static_cast<int>(_sets.size());
}

QString StockSymbolsSource::setTitle(int index) const {
    if (index < 0 || index >= setCount()) {
        return {};
    }
    return _sets[index]->title;
}

void StockSymbolsSource::load(Set& set) {
    if (!_widget) {
        return;
    }
    if (!set.document) {
        set.document = SPDocument::createNewDoc(set.filename.c_str());
    }
    if (set.document) {
        _widget->appendSymbols(make_symbols(set.document.get()));
    }
}

void StockSymbolsSource::loadSet(int index) {
    ++_generation;
    if (!_widget) {
        return;
    }
    if (index < 0 || index >= setCount()) {
        _widget->clearSymbols();
        return;
    }
    _widget->clearSymbols();
    load(*_sets[index]);
}

void StockSymbolsSource::loadNext(int generation, std::shared_ptr<int> next) {
    if (!_widget || generation != _generation || *next >= setCount()) {
        return;
    }
    load(*_sets[*next]);
    ++*next;
    QTimer::singleShot(0, this, [this, generation, next]() {
        loadNext(generation, next);
    });
}

void StockSymbolsSource::loadAll() {
    ++_generation;
    if (!_widget) {
        return;
    }
    _widget->clearSymbols();
    auto generation = _generation;
    auto next = std::make_shared<int>(0);
    QTimer::singleShot(0, this, [this, generation, next]() {
        loadNext(generation, next);
    });
}

} // namespace Linea::UI
