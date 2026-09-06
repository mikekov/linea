// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "patharray.h"

#include <QLabel>
#include <QWidget>
#include <utility>
#include <glibmm/i18n.h>

#include "document.h"
#include "inkscape.h"

#include "display/curve.h"
#include "live_effects/effect.h"
#include "live_effects/lpe-bspline.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"
#include "live_effects/lpe-spiro.h"
#include "live_effects/parameter/patharray.h"
#include "object/sp-shape.h"
#include "object/sp-text.h"
#include "object/uri.h"
#include "svg/stringstream.h"
#include "ui/clipboard.h"

namespace Inkscape::LivePathEffect {

PathArrayParam::PathArrayParam(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                               Inkscape::UI::Widget::Registry *wr, Effect *effect)
    : Parameter(label, tip, key, wr, effect)
{
    // refresh widgets on load to allow to remove the 
    // memory leak calling initui here
    param_effect->refresh_widgets = true;
    oncanvas_editable = true;
}

PathArrayParam::~PathArrayParam() {
    while (!_vector.empty()) {
        PathAndDirectionAndVisible *w = _vector.back();
        unlink(w);
    }
}

void PathArrayParam::param_set_default() {}

QWidget* PathArrayParam::param_newWidget()
{
    if (!widget_is_visible) return nullptr;

    auto label = new QLabel("NOT IMPLEMENTED");
    label->setAlignment(Qt::AlignCenter);
    auto f = label->font();
    f.setBold(true);
    f.setPointSize(f.pointSize() * 2);
    label->setFont(f);
    return label;
}

std::vector<SPObject *> PathArrayParam::param_get_satellites()
{
    std::vector<SPObject *> objs;
    for (auto const &iter : _vector) {
        if (iter && iter->ref.isAttached()) {
            SPObject *obj = iter->ref.getObject();
            if (obj) {
                objs.push_back(obj);
            }
        }
    }
    return objs;
}

void PathArrayParam::unlink(PathAndDirectionAndVisible *to)
{
    to->linked_modified_connection.disconnect();
    to->linked_release_connection.disconnect();
    to->ref.detach();
    to->_pathvector = Geom::PathVector();
    to->href.clear();

    for (auto iter = _vector.begin(); iter != _vector.end(); ++iter) {
        if (*iter == to) {
            PathAndDirectionAndVisible *w = *iter;
            _vector.erase(iter);
            delete w;
            return;
        }
    }
}

void PathArrayParam::start_listening()
{
    for (auto const &w : _vector) {
        linked_changed(nullptr,w->ref.getObject(), w);
    }
}

void PathArrayParam::linked_release(SPObject * /*release*/, PathAndDirectionAndVisible * to)
{
    if (to && param_effect->getLPEObj()) {
        to->linked_modified_connection.disconnect();
        to->linked_release_connection.disconnect();
    }
}

void PathArrayParam::linked_changed(SPObject * /*old_obj*/, SPObject *new_obj, PathAndDirectionAndVisible *to)
{
    if (to) {
        to->linked_modified_connection.disconnect();
        
        if (new_obj && is<SPItem>(new_obj)) {
            to->linked_release_connection.disconnect();
            to->linked_release_connection = new_obj->connectRelease(
                sigc::bind(sigc::mem_fun(*this, &PathArrayParam::linked_release), to));
            to->linked_modified_connection = new_obj->connectModified(
                sigc::bind(sigc::mem_fun(*this, &PathArrayParam::linked_modified), to));

            linked_modified(new_obj, SP_OBJECT_MODIFIED_FLAG, to);
        } else if (to->linked_release_connection.connected()){
            param_effect->getLPEObj()->requestModified(SP_OBJECT_MODIFIED_FLAG);
        }
    }  
}

void PathArrayParam::setPathVector(SPObject *linked_obj, guint /*flags*/, PathAndDirectionAndVisible *to)
{
    if (!to) {
        return;
    }
    std::optional<Geom::PathVector> curve;
    auto text = cast<SPText>(linked_obj);
    if (auto shape = cast<SPShape>(linked_obj)) {
        auto lpe_item = cast<SPLPEItem>(linked_obj);
        if (_from_original_d) {
            curve = ptr_to_opt(shape->curveForEdit());
        } else if (_allow_only_bspline_spiro && lpe_item && lpe_item->hasPathEffect()){
            curve = ptr_to_opt(shape->curveForEdit());
            PathEffectList lpelist = lpe_item->getEffectList();
            for (auto const &i : lpelist) {
                if (auto const lpeobj = i->lpeobject) {
                    auto const lpe = lpeobj->get_lpe();
                    if (auto bspline = dynamic_cast<Inkscape::LivePathEffect::LPEBSpline *>(lpe)) {
                        Geom::PathVector hp;
                        LivePathEffect::sp_bspline_do_effect(*curve, 0, hp, bspline->uniform);
                    } else if (dynamic_cast<Inkscape::LivePathEffect::LPESpiro *>(lpe)) {
                        LivePathEffect::sp_spiro_do_effect(*curve);
                    }
                }
            }
        } else {
            curve = ptr_to_opt(shape->curve());
        }
    } else if (text) {
        bool hidden = text->isHidden();
        if (hidden) {
            if (to->_pathvector.empty()) {
                text->setHidden(false);
                curve = text->getNormalizedBpath();
                text->setHidden(true);
            } else {
                if (!curve) {
                    curve.emplace();
                }
                curve = to->_pathvector;
            }
        } else {
            curve = text->getNormalizedBpath();
        }
    }

    if (!curve) {
        // curve invalid, set empty pathvector
        to->_pathvector = {};
    } else {
        to->_pathvector = *curve;
    }
}

void PathArrayParam::linked_modified(SPObject *linked_obj, guint flags, PathAndDirectionAndVisible *to)
{
    if (!_updating && flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG |
                 SP_OBJECT_CHILD_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        if (!to) {
            return;
        }
        setPathVector(linked_obj, flags, to);
        if (!param_effect->is_load || ownerlocator || (!SP_ACTIVE_DESKTOP && param_effect->isReady())) {
            param_effect->getLPEObj()->requestModified(SP_OBJECT_MODIFIED_FLAG);
        }
    }
}

bool PathArrayParam::param_readSVGValue(char const * const strvalue)
{
    if (strvalue) {
        while (!_vector.empty()) {
            PathAndDirectionAndVisible *w = _vector.back();
            unlink(w);
        }

        auto const strarray = g_strsplit(strvalue, "|", 0);
        bool write = false;
        for (auto iter = strarray; *iter != nullptr; ++iter) {
            if ((*iter)[0] == '#') {
                auto const substrarray = g_strsplit(*iter, ",", 0);
                SPObject * old_ref = param_effect->getSPDoc()->getObjectByHref(*substrarray);
                if (old_ref) {
                    SPObject * tmpsuccessor = old_ref->_tmpsuccessor;
                    Glib::ustring id = *substrarray;
                    if (tmpsuccessor && tmpsuccessor->getId()) {
                        id = tmpsuccessor->getId();
                        id.insert(id.begin(), '#');
                        write = true;
                    }
                    *(substrarray) = g_strdup(id.c_str());
                }
                PathAndDirectionAndVisible* w = new PathAndDirectionAndVisible((SPObject *)param_effect->getLPEObj());
                w->href = *substrarray;
                w->reversed = *(substrarray+1) != nullptr && (*(substrarray+1))[0] == '1';
                //Like this to make backwards compatible, new value added in 0.93
                w->visibled = *(substrarray+2) == nullptr || (*(substrarray+2))[0] == '1';
                w->linked_changed_connection = w->ref.changedSignal().connect(
                    sigc::bind(sigc::mem_fun(*this, &PathArrayParam::linked_changed), w));
                w->ref.attach(URI(w->href.c_str()));

                _vector.push_back(w);

                g_strfreev (substrarray);
            }
        }

        g_strfreev (strarray);

        if (write) {
            param_write_to_repr(param_getSVGValue().c_str());
        }

        return true;
        
    }
    return false;
}

Glib::ustring PathArrayParam::param_getSVGValue() const
{
    Inkscape::SVGOStringStream os;
    bool foundOne = false;
    for (auto const &iter : _vector) {
        if (foundOne) {
            os << "|";
        } else {
            foundOne = true;
        }
        os << iter->href.c_str() << "," << (iter->reversed ? "1" : "0") << "," << (iter->visibled ? "1" : "0");
    }
    return os.str();
}

Glib::ustring PathArrayParam::param_getDefaultSVGValue() const
{
    return "";
}

void PathArrayParam::update()
{
    for (auto const &iter : _vector) {
        SPObject *linked_obj = iter->ref.getObject();
        linked_modified(linked_obj, SP_OBJECT_MODIFIED_FLAG, iter);
    }
}

} // namespace Inkscape::LivePathEffect
