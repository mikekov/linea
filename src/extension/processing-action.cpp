// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Martin Owens
 *
 * Copyright (C) 2023 Edgewood
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "processing-action.h"

#include <glibmm/i18n.h>

#include "actions/actions-svg-processing.h"
#include "document.h"
#include "preferences.h"

#include "xml/attribute-record.h"
#include "xml/node.h"

namespace Inkscape::Extension {

ProcessingAction::ProcessingAction (Inkscape::XML::Node * in_repr)
{
    if (auto action = in_repr->firstChild()->content()) {
        _action_name = action;
    }
    for (const auto &iter : in_repr->attributeList()) {
        std::string name = g_quark_to_string(iter.key);
        std::string value = std::string(iter.value);
        if (name == "pref" && !value.empty()) {
            if (value.at(0) == '!') {
                _pref_default = false;
                value.erase(value.begin());
            }
            _pref = value;
        }
    }

    return;
}

/**
 * Check if the action should be run or not (prefs etc)
 */
bool ProcessingAction::is_enabled()
{
    // System preferences
    if (!_pref.empty()) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        return prefs->getBool(_pref, _pref_default);
    }
    return true;
}

/**
 * Run the action against the given document.
 */
void ProcessingAction::run(SPDocument *doc)
{
    if (!run_svg_processing_action(doc, _action_name.c_str())) {
        g_warning("Can't find action 'doc.%s'", _action_name.c_str());
    }
}

} /* namespace Inkscape::Extension */
