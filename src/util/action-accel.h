// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Action Accel
 * A simple tracker for accelerator keys associated to an action
 *
 * Authors:
 *   Rafael Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2022 the Authors.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef ACTION_ACCEL_H_SEEN
#define ACTION_ACCEL_H_SEEN

#include <set>
#include <vector>
#include <string>

#include <QKeySequence>

#include <sigc++/signal.h>
#include <sigc++/scoped_connection.h>

namespace Inkscape { struct KeyEvent; }

namespace Inkscape::Util {

/**
 * \brief The ActionAccel class stores the keyboard shortcuts for a given action
 * and automatically keeps track of changes in the keybindings.
 *
 * Additionally, a signal is emitted when the keybindings for the action change.
 *
 * In order to create an ActionAccel object, one must pass a string containing the
 * action name to the constructor. The object will automatically observe the
 * keybindings for that action, so you always get up-to-date keyboard shortcuts.
 * To check if a given key event triggers one of these keybindings, use `isTriggeredBy()`.
 *
 * Typical usage example:
 * \code{.cpp}
    auto accel = Inkscape::Util::ActionAccel("doc.undo");
    KeyEvent const &key = get_from_somewhere();
    if (accel.isTriggeredBy(key)) {
        ... // do stuff
    }
    accel.connectModified( []() { // This code will run when the user changes
                                  // the keybindings for this action.
                                } );
   \endcode
 */
class ActionAccel
{
private:
    sigc::signal<void ()> _we_changed; ///< Emitted when the keybindings for the action are changed
    sigc::scoped_connection _prefs_changed;    ///< To listen to changes to the keyboard shortcuts
    std::string _action;             ///< Name of the action
    std::set<QKeySequence> _accels;  ///< Stores the accelerator keys for the action

    /** Queries and updates the stored shortcuts, returning true if they have changed. */
    bool _query();

    /** Runs when the keyboard shortcut settings have changed */
    void _onShortcutsModified(const QString& action_id, const QKeySequence&);

public:
    /**
     * @brief Construct an ActionAccel object which will keep track of keybindings for a given action.
     * @param action_name - the name of the action to hold and observe the keybindings of.
     */
    ActionAccel(std::string action_name);

    /**
     * @brief Returns all keyboard shortcuts for the action.
     * @return a vector containing a QKeySequence for each of the keybindings present for the action.
     */
    std::vector<QKeySequence> getKeys() const
    {
        return std::vector<QKeySequence>(_accels.begin(), _accels.end());
    }

    /**
     * @brief Connects a void callback which will run whenever the keybindings for the action change.
     *        At the time when the callback runs, the values stored in the ActionAccel object will have
     *        already been updated. This means that the new keybindings can be queried by the callback.
     * @param slot - the sigc::slot representing the callback function.
     * @return the resulting sigc::connection.
     */
    sigc::connection connectModified(const sigc::slot<void()>& slot) { return _we_changed.connect(slot); }

    /**
     * @brief Checks whether a given key event triggers this action.
     * @param key - a KeyEvent containing key event data.
     * @return true if one of the keyboard shortcuts for the action is triggered by the passed event,
     *         false otherwise.
     */
    bool isTriggeredBy(KeyEvent const &key) const;

    /**
    * @brief Returns all keyboard shortcuts for the action in the form of text.
    * @return a vector containing a string for each of the keybindings present for the action.
    */
    std::vector<std::string> getShortcutText() const;
};

} // namespace Inkscape::Util

#endif // ACTION_ACCEL_H_SEEN
