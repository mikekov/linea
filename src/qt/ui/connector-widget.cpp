// SPDX-License-Identifier: GPL-2.0-or-later
/** @file ConnectorWidget implementation. */

#include "connector-widget.h"

#include <string>

#include <QPushButton>
#include <QSignalBlocker>

#include "actions/action-registry.h"
#include "conn-avoid-ref.h"
#include "document-undo.h"
#include "layer-manager.h"
#include "number-edit.h"
#include "object/sp-namedview.h"
#include "preferences.h"
#include "xml/node.h"
#include "ui/icon-names.h"
#include "selection.h"
#include "ui_connector-widget.h"
#include "ui/tools/connector-tool.h"
#include "object/sp-path.h"

namespace Linea::UI {

using Inkscape::Preferences;

ConnectorWidget::ConnectorWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::ConnectorWidget>()) {
    _ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto dummyPolicy = _ui->dummy->sizePolicy();
    dummyPolicy.setRetainSizeWhenHidden(true);
    _ui->dummy->setSizePolicy(dummyPolicy);

    auto configure = [](NumberEdit* spin, const char* path, double value, double minimum, double maximum, int decimals,
                        double step) {
        spin->setRange(minimum, maximum);
        spin->setDecimals(decimals);
        spin->setSingleStep(step);
        spin->setValue(Preferences::get()->getDouble(path, value));
        connect(spin, &NumberEdit::valueChanged, spin, [path](double newValue) {
            Preferences::get()->setDouble(path, newValue);
        });
    };
    configure(_ui->curvatureSpin, "/tools/connector/curvature", defaultConnCurvature, 0, 100, 2, 0.1);
    configure(_ui->spacingSpin, "/tools/connector/spacing", defaultConnSpacing, 0, 100, 2, 0.1);
    configure(_ui->lengthSpin, "/tools/connector/length", 100, 10, 1000, 0, 10);

    _ui->orthogonalButton->setChecked(Preferences::get()->getBool("/tools/connector/orthogonal", false));
    _ui->directedButton->setChecked(Preferences::get()->getBool("/tools/connector/directedlayout", false));
    _ui->overlapButton->setChecked(Preferences::get()->getBool("/tools/connector/avoidoverlaplayout", false));

    connect(_ui->orthogonalButton, &QPushButton::toggled, this, &ConnectorWidget::orthogonalToggled);
    connect(_ui->directedButton, &QPushButton::toggled, this, [](bool value) {
        Preferences::get()->setBool("/tools/connector/directedlayout", value);
    });
    connect(_ui->overlapButton, &QPushButton::toggled, this, [](bool value) {
        Preferences::get()->setBool("/tools/connector/avoidoverlaplayout", value);
    });
    connect(_ui->curvatureSpin, &NumberEdit::valueChanged, this, &ConnectorWidget::curvatureChanged);
    connect(_ui->spacingSpin, &NumberEdit::valueChanged, this, &ConnectorWidget::spacingChanged);

    connect(_ui->avoidButton, &QPushButton::clicked, this, [this] {
        if (_desktop) Inkscape::UI::Tools::cc_selection_set_avoid(_desktop, true);
    });
    connect(_ui->ignoreButton, &QPushButton::clicked, this, [this] {
        if (_desktop) Inkscape::UI::Tools::cc_selection_set_avoid(_desktop, false);
    });
    connect(_ui->graphButton, &QPushButton::clicked, this, [] {
        if (auto action = ActionRegistry::get().action("object-rearrange-graph")) action->trigger();
    });
}

ConnectorWidget::~ConnectorWidget() {
    _selectionChanged.disconnect();
    if (_repr) {
        _repr->removeObserver(*this);
        Inkscape::GC::release(_repr);
    }
}

void ConnectorWidget::setDesktop(SPDesktop* desktop) {
    _selectionChanged.disconnect();
    if (_repr) {
        _repr->removeObserver(*this);
        Inkscape::GC::release(_repr);
        _repr = nullptr;
    }
    _desktop = desktop;
    if (!_desktop) return;

    _selectionChanged = _desktop->getSelection()->connectChanged([this](auto) { selectionChanged(); });
    _repr = _desktop->getNamedView()->getRepr();
    Inkscape::GC::anchor(_repr);
    _repr->addObserver(*this);
    selectionChanged();
}

void ConnectorWidget::orthogonalToggled(bool orthogonal) {
    if (!_desktop || _updating) return;

    auto doc = _desktop->getDocument();
    if (!Inkscape::DocumentUndo::getUndoSensitive(doc)) return;

    _updating = true;
    bool modified = false;
    const auto value = orthogonal ? "orthogonal" : "polyline";
    for (auto item : _desktop->getSelection()->items()) {
        if (Inkscape::UI::Tools::cc_item_is_connector(item)) {
            item->setAttribute("inkscape:connector-type", value);
            item->getAvoidRef().handleSettingChange();
            modified = true;
        }
    }
    _updating = false;

    if (!modified) {
        Preferences::get()->setBool("/tools/connector/orthogonal", orthogonal);
    } else {
        Inkscape::DocumentUndo::done(doc, orthogonal ? RC_("Undo", "Set connector type: orthogonal")
                                                     : RC_("Undo", "Set connector type: polyline"),
                                     INKSCAPE_ICON("draw-connector"));
    }
}

void ConnectorWidget::curvatureChanged(double curvature) {
    if (!_desktop || _updating) return;

    auto doc = _desktop->getDocument();
    if (!Inkscape::DocumentUndo::getUndoSensitive(doc)) return;

    _updating = true;
    bool modified = false;
    for (auto item : _desktop->getSelection()->items()) {
        if (Inkscape::UI::Tools::cc_item_is_connector(item)) {
            const auto value = std::to_string(curvature);
            item->setAttribute("inkscape:connector-curvature", value.c_str());
            item->getAvoidRef().handleSettingChange();
            modified = true;
        }
    }
    _updating = false;

    if (!modified) {
        Preferences::get()->setDouble("/tools/connector/curvature", curvature);
    } else {
        Inkscape::DocumentUndo::done(doc, RC_("Undo", "Change connector curvature"), INKSCAPE_ICON("draw-connector"));
    }
}

void ConnectorWidget::spacingChanged(double spacing) {
    if (!_desktop || _updating) return;

    auto doc = _desktop->getDocument();
    if (!Inkscape::DocumentUndo::getUndoSensitive(doc)) return;

    auto repr = _desktop->getNamedView()->getRepr();
    if (!repr->attribute("inkscape:connector-spacing") && spacing == defaultConnSpacing) return;

    _updating = true;
    repr->setAttributeCssDouble("inkscape:connector-spacing", spacing);
    _desktop->getNamedView()->updateRepr();
    bool modified = false;
    for (auto item : get_avoided_items(_desktop->layerManager().currentRoot(), _desktop)) {
        auto transform = Geom::identity();
        avoid_item_move(&transform, item);
        modified = true;
    }
    _updating = false;

    if (modified) {
        Inkscape::DocumentUndo::done(doc, RC_("Undo", "Change connector spacing"), INKSCAPE_ICON("draw-connector"));
    }
}

void ConnectorWidget::selectionChanged() {
    if (!_desktop || _updating) return;

    if (auto path = cast<SPPath>(_desktop->getSelection()->singleItem())) {
        QSignalBlocker orthogonalBlocker(_ui->orthogonalButton);
        QSignalBlocker curvatureBlocker(_ui->curvatureSpin);
        _ui->orthogonalButton->setChecked(path->connEndPair.isOrthogonal());
        _ui->curvatureSpin->setValue(path->connEndPair.getCurvature());
    }
}

void ConnectorWidget::notifyAttributeChanged(Inkscape::XML::Node&, GQuark name, Inkscape::Util::ptr_shared,
                                              Inkscape::Util::ptr_shared) {
    static const auto spacingQuark = g_quark_from_static_string("inkscape:connector-spacing");
    if (name != spacingQuark || !_desktop || _updating) return;

    QSignalBlocker blocker(_ui->spacingSpin);
    _ui->spacingSpin->setValue(_desktop->getNamedView()->connector_spacing);
}

} // namespace Linea::UI
