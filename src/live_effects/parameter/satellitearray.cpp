// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Jabiertxof <jabier.arraiza@marker.es>
 * this class handle satellites of a lpe as a parameter
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "satellitearray.h"

#include <QLabel>
#include <QWidget>
#include <algorithm>
#include <string>
#include <utility>
#include <glibmm/i18n.h>

#include "inkscape.h"
#include "selection.h"
#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "object/sp-lpe-item.h"
#include "ui/clipboard.h"

namespace Inkscape::LivePathEffect {

SatelliteArrayParam::SatelliteArrayParam(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                                         Inkscape::UI::Widget::Registry *wr, Effect *effect, bool visible)
    : ArrayParam<std::shared_ptr<SatelliteReference>>(label, tip, key, wr, effect)
    , _visible(visible)
{
    param_widget_is_visible(_visible);

    if (_visible) {
        oncanvas_editable = true;
    }
}

SatelliteArrayParam::~SatelliteArrayParam() {
    _vector.clear();
    quit_listening();
}

void SatelliteArrayParam::start_listening()
{
    quit_listening();
    for (auto const &ref : _vector) {
        if (ref && ref->isAttached()) {
            auto item = cast<SPItem>(ref->getObject());
            if (item) {
                linked_connections.emplace_back(item->connectRelease(
                    sigc::hide(sigc::mem_fun(*this, &SatelliteArrayParam::updatesignal))));
                linked_connections.emplace_back(item->connectModified(
                    sigc::mem_fun(*this, &SatelliteArrayParam::linked_modified)));
                linked_connections.emplace_back(item->connectTransformed(
                    sigc::hide(sigc::hide(sigc::mem_fun(*this, &SatelliteArrayParam::updatesignal)))));
                linked_connections.emplace_back(ref->changedSignal().connect(
                    sigc::hide(sigc::hide(sigc::mem_fun(*this, &SatelliteArrayParam::updatesignal)))));
            }
        }
    }
}

void SatelliteArrayParam::linked_modified(SPObject *linked_obj, guint flags) {
    if (!_updating && (!SP_ACTIVE_DESKTOP || SP_ACTIVE_DESKTOP->getSelection()->includes(linked_obj)) &&
        (!param_effect->is_load || ownerlocator || !SP_ACTIVE_DESKTOP ) && 
        param_effect->_lpe_action == LPE_NONE &&
        param_effect->isReady() &&
        flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG |
                 SP_OBJECT_CHILD_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG))
    {
        param_effect->processObjects(LPE_UPDATE);
    }
}

void SatelliteArrayParam::updatesignal()
{
    if (!_updating && 
        (!param_effect->is_load || ownerlocator || !SP_ACTIVE_DESKTOP ) && 
        param_effect->_lpe_action == LPE_NONE 
        && param_effect->isReady()) 
    {
        param_effect->processObjects(LPE_UPDATE);
    }
}

void SatelliteArrayParam::quit_listening()
{
    linked_connections.clear();
};

bool SatelliteArrayParam::param_readSVGValue(char const * const strvalue)
{
    if (strvalue) {
        bool changed = !linked_connections.size() || !param_effect->is_load;
        if (!ArrayParam::param_readSVGValue(strvalue)) {
            return false;
        }
        auto lpeitems = param_effect->getCurrrentLPEItems();
        if (!lpeitems.size() && !param_effect->is_applied && !param_effect->getSPDoc()->isSeeking()) {
            size_t pos = 0;
            for (auto const &w : _vector) {
                if (w) {
                    SPObject * tmp = w->getObject();
                    if (tmp) {
                        SPObject * tmpsuccessor = tmp->_tmpsuccessor;
                        unlink(tmp);
                        if (tmpsuccessor && tmpsuccessor->getId()) {
                            link(tmpsuccessor,pos);
                        }
                    }
                }
                pos ++;
            }
            param_write_to_repr(param_getSVGValue().c_str());
        }
        if (changed) {
            start_listening();
        }
        return true;
    }
    return false;
}

QWidget* SatelliteArrayParam::param_newWidget()
{
    if (!widget_is_visible) return nullptr;

    // this parameter type is used by some LPEs, but it is invisible
    // *except* one: measure segments
    auto label = new QLabel("NOT IMPLEMENTED");
    label->setAlignment(Qt::AlignCenter);
    auto f = label->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() * 2);
    label->setFont(f);
    return label;
}

std::vector<SPObject *> SatelliteArrayParam::param_get_satellites()
{
    std::vector<SPObject *> objs;
    for (auto &iter : _vector) {
        if (iter && iter->isAttached()) {
            SPObject *obj = iter->getObject();
            if (obj) {
                objs.push_back(obj);
            }
        }
    }
    return objs;
}

/*
 * This function link a satellite writing into XML directly
 * @param obj: object to link
 * @param obj: position in vector
 */
void SatelliteArrayParam::link(SPObject *obj, size_t pos)
{
    if (obj && obj->getId()) {
        Glib::ustring itemid = "#";
        itemid += obj->getId();
        auto satellitereference =
            std::make_shared<SatelliteReference>(param_effect->getLPEObj(), _visible);
        try {
            satellitereference->attach(Inkscape::URI(itemid.c_str()));
            if (_visible) {
                satellitereference->setActive(true);
            }
            if (_vector.size() == pos || pos == Glib::ustring::npos) {
                _vector.push_back(std::move(satellitereference));
            } else {
                _vector[pos] = std::move(satellitereference);
            }
        } catch (Inkscape::BadURIException &e) {
            g_warning("%s", e.what());
            satellitereference->detach();
        }
    }
}

void SatelliteArrayParam::unlink(SPObject *obj)
{
    if (!obj) {
        return;
    }

    auto const it = std::find_if(_vector.begin(), _vector.end(),
                                 [=](auto const &w){ return w && w->getObject() == obj; });
    if (it != _vector.end()) it->reset();
}

void SatelliteArrayParam::unlink(std::shared_ptr<SatelliteReference> const &to)
{
    unlink(to->getObject());
}

void SatelliteArrayParam::clear()
{
    _vector.clear();
}

} // namespace Inkscape::LivePathEffect
