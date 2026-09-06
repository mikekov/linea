// SPDX-License-Identifier: GPL-2.0-or-later

#include "element-edit.h"
#include "desktop.h"
#include "selection.h"
#include "variant-visitor.h"
#include "text-utils.h"

using namespace Inkscape::UI;

namespace Linea::Util {

// Default delegate: applies every operation to all items in the desktop selection.
class DefaultPaintEditDelegate : public Util::PaintEditDelegate {
    SPDesktop* _desktop = nullptr;
    unsigned int _tag;
public:
    explicit DefaultPaintEditDelegate(unsigned int tag) : _tag(tag) {}
    void set_desktop(SPDesktop* d) override { _desktop = d; }
    SPPaintServer* apply(const Op& op) override {
        if (!_desktop) return nullptr;
        auto sel = _desktop->getSelection();
        if (!sel) return nullptr;
        SPPaintServer* server = nullptr;
        for (auto item : sel->items()) {
            auto s = Util::apply_paint_op_to_item(item, op, _desktop, _tag);
            if (!server) server = s;
        }
        return server;
    }
};

// Default delegate: applies every operation to all items in the desktop selection.
// class DefaultPaintEditDelegate : public Util::PaintEditDelegate {
//     SPDesktop* _desktop = nullptr;
//     unsigned int _tag;
// public:
//     explicit DefaultPaintEditDelegate(unsigned int tag) : _tag(tag) {}
//     void set_desktop(SPDesktop* d) override { _desktop = d; }
//     SPPaintServer* apply_to_selection(const PaintEditDelegate::Op& op, SPDesktop* desktop, unsigned int tag) {
//         if (!desktop) return nullptr;
//         auto sel = desktop->getSelection();
//         if (!sel) return nullptr;
//         SPPaintServer* server = nullptr;
//         for (auto item : sel->items()) {
//             auto s = Util::apply_paint_op_to_item(item, op, desktop, tag);
//             if (!server) server = s;
//         }
//         return server;
//     }
// };

// Delegate for multi-object editing: iterates every item in the current selection
// and applies the operation to each one individually.
// class MultiObjPaintDelegate : public Util::PaintEditDelegate {
//     std::function<SPDesktop*()> _get_desktop;
//     unsigned int _tag;
// public:
//     MultiObjPaintDelegate(std::function<SPDesktop*()> get_desktop, unsigned int tag)
//         : _get_desktop(std::move(get_desktop)), _tag(tag) {}

    // SPPaintServer* apply(const PaintEditDelegate::Op& op) override {
    //     auto desktop = _get_desktop();
    //     if (!desktop) return nullptr;
    //     auto sel = desktop->getSelection();
    //     if (!sel) return nullptr;
    //     SPPaintServer* server = nullptr;
    //     for (auto item : sel->items()) {
    //         auto s = Util::apply_paint_op_to_item(item, op, desktop, _tag);
    //         if (!server) server = s;
    //     }
    //     return server;
    // }
// };

// Delegate for text items: CSS edits go through apply_text_css (respecting text-tool
// subselection); all other operations are applied directly to the text item.
class TextPaintDelegate : public Util::PaintEditDelegate {
    std::function<SPItem*()>            _get_item;
    std::function<SPDocument*()>        _get_doc;
    std::function<SPDesktop*()>         _get_desktop;
    std::function<Tools::TextTool*()>   _get_tool;
    unsigned int _tag;
public:
    TextPaintDelegate(std::function<SPItem*()> get_item,
                      std::function<SPDocument*()> get_doc,
                      std::function<SPDesktop*()> get_desktop,
                      std::function<Tools::TextTool*()> get_tool,
                      unsigned int tag)
        : _get_item(std::move(get_item))
        , _get_doc(std::move(get_doc))
        , _get_desktop(std::move(get_desktop))
        , _get_tool(std::move(get_tool))
        , _tag(tag)
    {}

    SPPaintServer* apply(const Op& op) override {
        auto item    = _get_item();
        auto desktop = _get_desktop();
        if (!item || !_get_doc()) return nullptr;
        unsigned int t = _tag;
        SPPaintServer* server = nullptr;
        std::visit(VariantVisitor{
            [item, this, &server](const CssOp& o) {
                apply_text_css(item, _get_tool(), o.css.get());
            },
            [item, desktop, t, &server](const GradientOp& o)    { server = Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t, &server](const PatternOp& o)     { server = Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t, &server](const HatchOp& o)       { server = Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t, &server](const MeshOp& o)        { server = Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t, &server](const SwatchOp& o)      { server = Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t](const StrokeWidthOp& o) { Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t](const DashOp& o)        { Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t](const OpacityOp& o)     { Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t](const BlendModeOp& o)   { Util::apply_paint_op_to_item(item, o, desktop, t); },
            [item, desktop, t](const VisibilityOp& o)  { Util::apply_paint_op_to_item(item, o, desktop, t); },
        }, op);
        return server;
    }
};

ElementEdit::ElementEdit(unsigned int tag) : _tag(tag) {
}

void ElementEdit::set_desktop(SPDesktop* desktop) {
    _desktop = desktop;
}

SPPaintServer* ElementEdit::apply(const Op& op) {
    // return apply_to_selection(op`, _desktop, _tag);
    // SPPaintServer* apply(const Op& op) override {
        if (!_desktop) return nullptr;

        auto sel = _desktop->getSelection();
        if (!sel) return nullptr;

        SPPaintServer* server = nullptr;
        for (auto item : sel->items()) {
            auto s = Util::apply_paint_op_to_item(item, op, _desktop, _tag);
            if (!server) server = s;
        }
        return server;
    // }
}

} // namespace Linea::Util
