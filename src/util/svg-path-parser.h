// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_SVG_PATH_FORMATTER_H
#define INK_SVG_PATH_FORMATTER_H

#include <glibmm/ustring.h>

namespace Inkscape {
class SvgPathParser
{
public:
    static Glib::ustring prettify_svgd(Glib::ustring const &d);

private:
    // Returns the number of arguments a path command expects
    static int get_command_arg_count(char cmd);
};

} // Namespace Inkscape

#endif // INK_SVG_PATH_FORMATTER_H
