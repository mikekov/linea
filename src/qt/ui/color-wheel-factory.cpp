// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Qt color wheel factory.
 */

#include "color-wheel-factory.h"
#include "color-plate.h"
#include "colors/spaces/base.h"
#include "colors/spaces/enum.h"

#include <chrono>
#include <optional>
#include <QDebug>
#include <QImage>
#include <QPainter>

namespace Linea::UI {

using namespace Inkscape::Colors::Space;
using namespace Inkscape::Colors;

constexpr bool TEST_TIMING = false;

class FastColorPlate : public ColorPlate {
    Q_OBJECT
public:
    FastColorPlate(Type source, Type plate, int fixed_channel, int var_channel1, int var_channel2, bool disc, QWidget* parent)
        : ColorPlate(parent)
        , _source(source)
        , _plate(plate)
        , _fixed_channel(fixed_channel)
        , _var_channel1(var_channel1)
        , _var_channel2(var_channel2)
    {
        setDisc(disc);
    }

    void setColor(const Color& color) override {
        if (isDragging()) return;

        auto copy = color.converted(_plate);
        auto dest = copy.value_or(Color{_plate, {0, 0, 0}});

        // When the source's hue convention matches the plate's, the stored
        // hue is authoritative: the converted hue carries residue that jumps
        // arbitrarily for grays and flips the 0/360 representation at the
        // seam. Otherwise keep the last hue shown for achromatic colors,
        // whose converted hue is meaningless.
        auto const type = color.getSpace() ? color.getSpace()->getType() : Type::NONE;
        if (auto const hue_ch = plateHueChannel(type)) {
            dest.set(0, color[*hue_ch]);
        } else if (plateAchromatic(dest) && _last_hue >= 0.0) {
            dest.set(0, _last_hue);
        }
        _last_hue = dest[0];

        setBaseColor(dest, _fixed_channel, _var_channel1, _var_channel2);
        moveIndicatorTo(dest);
    }

    QWidget* getWidget() override { return this; }

    sigc::connection connectColorChanged(sigc::slot<void(const Color&)> cb) override {
        return ColorPlate::connectColorChanged([this, cb](const Color& c) {
            auto color = c.converted(_source);
            if (color) {
                // When the plate holds hue fixed (rectangular plate), that
                // hue is authoritative: the conversion residue makes the hue
                // field wobble or flip across the 0/360 seam. Same for
                // achromatic picks, where the converted hue is meaningless.
                if (_fixed_channel == 0 || plateAchromatic(c)) {
                    if (auto const hue_ch = plateHueChannel(_source)) {
                        color->set(*hue_ch, c[0]);
                    }
                }
                cb(*color);
            } else {
                qWarning("Color conversion from type %d to type %d failed.", int(_plate), int(_source));
            }
        });
    }

private:
    // Hue is meaningless at zero saturation and at black.
    static bool plateAchromatic(const Color& dest) {
        return dest[1] <= ACHROMATIC_CHROMA || dest[2] <= ACHROMATIC_CHROMA;
    }

    // Index of the hue channel in a color of the given space, when its hue
    // convention matches the plate's (HSL/HSV <-> HSV plate,
    // OKHSL/OKLCH <-> OKHSV plate).
    std::optional<int> plateHueChannel(Type type) const {
        switch (type) {
        case Type::HSL:
        case Type::HSV:
            if (_plate == Type::HSV) return 0;
            break;
        case Type::OKHSL:
            if (_plate == Type::OKHSV) return 0;
            break;
        case Type::OKLCH:
            if (_plate == Type::OKHSV) return 2;
            break;
        default:
            break;
        }
        return {};
    }

    Type _source;
    Type _plate;
    int _fixed_channel;
    int _var_channel1;
    int _var_channel2;
    double _last_hue = -1.0; // last hue rendered on the plate; -1 before the first setColor
};

static ColorWheel* createPlate(Type source, Type plate, bool disc, QWidget* parent) {
    if (disc) {
        auto value = 2; // if value changes, color wheel needs to be rebuilt
        auto hue = 0;   // vary hue with angle (while painting the disc)
        auto sat = 1;   // vary saturation with distance from the center of the disc (while painting the disc)
        return new FastColorPlate(source, plate, value, hue, sat, disc, parent);
    } else {
        auto hue = 0;   // hue is fixed; it's a single hue rectangular plate
        auto sat = 1;
        auto value = 2;
        return new FastColorPlate(source, plate, hue, sat, value, disc, parent);
    }
}

static Type plateTypeFor(Type source) {
    switch (source) {
    case Type::OKHSL:
    case Type::OKLCH:
        return Type::OKHSV;
    default:
        return Type::HSV;
    }
}

static std::pair<ColorWheel*, bool> createHelper(Type type, bool create, bool disc, QWidget* parent) {
    switch (type) {
    case Type::HSL:
    case Type::HSLUV:
    case Type::OKHSL:
    case Type::OKLCH:
    case Type::HSV:
    case Type::RGB:
    case Type::CMYK:
        break;
    default:
        return {nullptr, false};
    }

    if (!create) return {nullptr, true};

    ColorWheel* wheel = createPlate(type, plateTypeFor(type), disc, parent);

    // Speed test — use this to evaluate how quickly we can rebuild a color wheel
    if (TEST_TIMING) {
        QImage offscreen(1024, 1024, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&offscreen);

        for (auto w : {wheel}) {
            auto plate = static_cast<ColorPlate*>(w->getWidget());
            auto old_time = std::chrono::high_resolution_clock::now();
            Color color(Type::OKHSL, {0.5, 0.5, 0.5});
            for (int i = 0; i < 100; ++i) {
                color.set(0, i / 100.0);
                w->setColor(color);
                plate->render(&painter);
            }
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - old_time);
            qDebug("render time for test wheel: %d ms", static_cast<int>(elapsed.count()));
        }
    }

    return {wheel, true};
}

ColorWheel* createColorWheel(Type type, bool disc, QWidget* parent) {
    auto [wheel, _] = createHelper(type, true, disc, parent);
    return wheel;
}

bool canCreateColorWheel(Type type) {
    auto [_, ok] = createHelper(type, false, true, nullptr);
    return ok;
}

} // namespace Linea::UI

#include "color-wheel-factory.moc"
