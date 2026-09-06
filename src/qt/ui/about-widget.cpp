// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * About Linea popup widget implementation.
 */

#include "about-widget.h"

#include <QAbstractButton>
#include <QPainter>

#include <glibmm/fileutils.h>

#include "document.h" // IWYU pragma: keep
#include "inkscape-version-info.h"
#include "io/file.h"
#include "io/resource.h"
#include "ui/svg-renderer.h"
#include "ui_about-widget.h"

namespace Linea::UI {

AboutWidget::AboutWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::AboutWidget>()) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    _ui->setupUi(this);
    connect(_ui->closeButton, &QAbstractButton::clicked, this, &QWidget::hide);

    auto filename = Inkscape::IO::Resource::get_path_string(
        Inkscape::IO::Resource::SYSTEM, Inkscape::IO::Resource::UIS, "about.svg");
    if (filename.empty()) {
        qWarning() << "About SVG not found";
        return;
    }

    try {
        auto svg = Glib::file_get_contents(filename);
        Glib::ustring const version = Linea::linea_version();
        Glib::ustring const placeholder = "{VERSION}";
        Glib::ustring::size_type position = 0;
        while ((position = svg.find(placeholder, position)) != Glib::ustring::npos) {
            svg.replace(position, placeholder.size(), version);
            position += version.size();
        }

        auto document = ink_file_open(std::span<char const>(svg.data(), svg.size()));
        if (!document) {
            qWarning() << "Failed to parse About SVG";
            return;
        }

        Inkscape::svg_renderer renderer(*document);
        auto scale = devicePixelRatioF();
        auto image = renderer.render_qimage(scale);
        if (image.isNull()) {
            qWarning() << "Failed to render About SVG";
            return;
        }

        image.setDevicePixelRatio(scale);
        _background = std::move(image);
        setFixedSize(_background.deviceIndependentSize().toSize());
    } catch (std::exception const& e) {
        qWarning() << "Exception rendering About SVG:" << e.what();
        // Keep the standard widget background until the About SVG is available.
    }
}

void AboutWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    if (_background.isNull()) {
        painter.fillRect(rect(), palette().window());
        return;
    }

    painter.drawImage(rect(), _background);
}

AboutWidget::~AboutWidget() = default;

} // namespace Linea::UI
