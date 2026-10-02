// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt6 entry point for minimal Inkscape viewer.
 */

#include <QApplication>
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

#include "auto-save.h"
#include "document.h"
#include "helper/gettext.h"
#include "inkgc/gc-core.h"
#include "inkscape.h"
#include "io/file.h"
#include "io/file-export-cmd.h"
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
        // setenv("INKSCAPE_FORCE_BUNDLE_ENV", "1", 1);
        set_xdg_env();
        Inkscape::Application::create(true);
        LineaApplication::create();
        Inkscape::AutoSave::getInstance().init(&LineaApplication::instance());

        auto setTheme = []{
            auto theme = static_cast<Linea::UI::ThemeMode>(
                LineaApplication::instance().settings().value("dark-theme", 0).toInt());
            auto scheme = QGuiApplication::styleHints()->colorScheme();
            bool followSystem = (theme == Linea::UI::ThemeMode::System);
            Linea::UI::setApplicationTheme(
                followSystem ? scheme == Qt::ColorScheme::Dark : (theme == Linea::UI::ThemeMode::Dark),
                followSystem);
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
#if 0
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

        // Headless export: `linea input.svg -o out.png -w 32 -h 32` renders
        // without opening a window. PNG size comes from -w/-h (independent
        // unless both given), otherwise the document size at -d dpi.
        QCommandLineOption exportFilename({"o", "export-filename"},
            "Export the input file to this path and exit without a GUI", "file");
        QCommandLineOption exportWidth({"w", "export-width"},
            "Export width in pixels (PNG)", "px");
        QCommandLineOption exportHeight("export-height",
            "Export height in pixels (PNG)", "px");
        QCommandLineOption exportDpi({"d", "export-dpi"},
            "Export resolution in DPI", "dpi");
        parser.addOptions({exportFilename, exportWidth, exportHeight, exportDpi});

        parser.process(app);

        QStringList args = parser.positionalArguments();

        if (parser.isSet(exportFilename)) {
            if (args.isEmpty()) {
                std::cerr << "Linea: --export-filename requires an input file" << std::endl;
                LINEA_APP.shutdown();
                Inkscape::Util::StaticsBin::get().destroy();
                return 1;
            }
            auto file = Gio::File::create_for_path(args.first().toStdString());
            auto [document, error] = ink_file_open(file);
            if (!document) {
                std::cerr << "Linea: failed to load " << args.first().toStdString() << std::endl;
                LINEA_APP.shutdown();
                Inkscape::Util::StaticsBin::get().destroy();
                return 1;
            }
            InkFileExportCmd exporter;
            exporter.export_filename = parser.value(exportFilename).toStdString();
            exporter.export_width = parser.value(exportWidth).toInt();
            exporter.export_height = parser.value(exportHeight).toInt();
            exporter.export_dpi = parser.value(exportDpi).toDouble();
            exporter.export_overwrite = true;
            exporter.do_export(document.get(), args.first().toStdString());
            LINEA_APP.shutdown();
            Inkscape::Util::StaticsBin::get().destroy();
            return 0;
        }

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
