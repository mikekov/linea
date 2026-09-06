// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt6 entry point for minimal Inkscape viewer.
 */

#include <QApplication>
// #include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <QtGui/qrgb.h>
#include <QSurfaceFormat>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QStringList>
#include <iostream>

#include <glib.h>
#include <glibmm/init.h>
#include <glibmm/miscutils.h>
#include <giomm/file.h>

#include "helper/gettext.h"
#include "inkgc/gc-core.h"
#include "inkscape.h"
#include "linea-application.h"
#include "path-prefix.h"
#include "extension/init.h"
#include "preferences.h"
#include "theme.h"
#include "util/statics.h"

int main(int argc, char* argv[]) {
    try {
        // Required before QApplication: share GL contexts so QOpenGLWidget can composite
        // into the parent window on macOS (fixes "Failed to create wrapper texture").
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        // QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, true);

        QSurfaceFormat format;
        format.setVersion(3, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        format.setStencilBufferSize(8);
        QSurfaceFormat::setDefaultFormat(format);

        // GLib threading is automatically initialized in modern GLib
        Inkscape::GC::init();
        Inkscape::initialize_gettext();

        QGuiApplication::setDesktopSettingsAware(true);
        QApplication app(argc, argv);

        QFile file(":/styles/styles.qss");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            app.setStyleSheet(QString::fromUtf8(file.readAll()));
        }
        else {
            std::cerr << "Failed to load styles.qss" << std::endl;
        }

        Glib::init();
        // Qt bundles don't have girepository-1.0, so force bundle env setup
        setenv("INKSCAPE_FORCE_BUNDLE_ENV", "1", 1);
        set_xdg_env();
        g_message("get_inkscape_datadir() = %s", get_inkscape_datadir());
        g_message("XDG_DATA_DIRS = %s", Glib::getenv("XDG_DATA_DIRS").empty() ? "(not set)" : Glib::getenv("XDG_DATA_DIRS").c_str());
        Inkscape::Application::create(true);
        LineaApplication::create();

        auto setTheme = []{
            int theme = LineaApplication::instance().settings().value("dark-theme", 0).toInt();
            auto scheme = QGuiApplication::styleHints()->colorScheme();
            bool followSystem = (theme == 0);
            Linea::UI::setApplicationTheme(followSystem ? scheme == Qt::ColorScheme::Dark : (theme == 1), followSystem);
        };
        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                         &app, [&setTheme](Qt::ColorScheme scheme) {
            setTheme();
        });
        setTheme();

        // Initialize extensions (similar to InkscapeApplication::on_startup)
        Inkscape::Extension::init();
        // Inkscape::Extension::shallow_init();
        app.setApplicationName("LineaDraw");
        app.setApplicationDisplayName("Linea Draw");
#if 1
        {
            auto palette = app.palette();
            struct { QPalette::ColorRole role; const char* name; } roles[] = {
                { QPalette::Window,          "Window"          },
                { QPalette::WindowText,      "WindowText"      },
                { QPalette::Base,            "Base"            },
                { QPalette::AlternateBase,   "AlternateBase"   },
                { QPalette::Text,            "Text"            },
                { QPalette::BrightText,      "BrightText"      },
                { QPalette::Button,          "Button"          },
                { QPalette::ButtonText,      "ButtonText"      },
                { QPalette::Highlight,       "Highlight"       },
                { QPalette::HighlightedText, "HighlightedText" },
                { QPalette::Link,            "Link"            },
                { QPalette::LinkVisited,     "LinkVisited"     },
                { QPalette::Light,           "Light"           },
                { QPalette::Midlight,        "Midlight"        },
                { QPalette::Mid,             "Mid"             },
                { QPalette::Dark,            "Dark"            },
                { QPalette::Shadow,          "Shadow"          },
                { QPalette::ToolTipBase,     "ToolTipBase"     },
                { QPalette::ToolTipText,     "ToolTipText"     },
                { QPalette::PlaceholderText, "PlaceholderText" },
                { QPalette::Accent,          "Accent"          },
            };
            std::cerr << "=== App Palette (Active group) ===" << std::endl;
            for (auto& r : roles) {
                QColor c = palette.color(QPalette::Active, r.role);
                std::cerr << std::left << std::setw(20) << r.name
                          << " " << c.name(QColor::HexArgb).toStdString() << std::endl;
            }
        }
#endif

        // Destroy all windows and documents while QApplication is still alive.
        // LineaApplication owns LineaWindow instances (Qt widgets); if we let
        // them be destroyed by LineaApplication's static destructor after
        // QApplication is gone, Qt's widget infrastructure is already torn down.
        QObject::connect(&app, &QApplication::aboutToQuit, []() {
            LINEA_APP.shutdown();
        });

        QCommandLineParser parser;
        parser.setApplicationDescription("Linea Draw SVG editor");
        parser.addHelpOption();
        parser.addPositionalArgument("file", "SVG file to open (optional)");

        parser.process(app);

        QStringList args = parser.positionalArguments();

        if (!args.isEmpty()) {
            QString filePath = args.first();
            QFileInfo fileInfo(filePath);

            if (fileInfo.exists()) {
                auto file = Gio::File::create_for_path(filePath.toStdString());
                LINEA_APP.openDocument(file);
            } else {
                std::cerr << "File not found: " << filePath.toStdString() << std::endl;
            }
        }

        // Always create a window — either with the requested file, or a blank
        // document if the file was missing or no argument was given.
        if (!LINEA_APP.get_active_window()) {
            LINEA_APP.createWindow();
        }

        if (!LINEA_APP.get_active_window()) {
            std::cerr << "Linea: Failed to open document" << std::endl;
            LINEA_APP.shutdown();
            Inkscape::Util::StaticsBin::get().destroy();
            return 1;
        }

        int result = app.exec();

        Inkscape::Preferences::get()->save();
        Inkscape::Util::StaticsBin::get().destroy();

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Linea: Startup error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Linea: Unknown startup error occurred" << std::endl;
        return 1;
    }
}
