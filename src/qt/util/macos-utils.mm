// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * macOS-specific application helpers
 */

#include "macos-utils.h"

#import <AppKit/AppKit.h>

namespace Linea::UI {

void activateApplication() {
    if (@available(macOS 14, *)) {
        [NSApp activate];
    } else {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

} // namespace Linea::UI
