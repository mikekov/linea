// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_WIDGET_EVENTS_KEYVALS_H
#define INKSCAPE_UI_WIDGET_EVENTS_KEYVALS_H

/**
 * @file
 * Toolkit-agnostic keyval constants.
 *
 * These values are identical to the corresponding GDK_KEY_* definitions
 * in gdkkeysyms.h.  The Qt event converter already maps Qt keys to these
 * numeric values, so keeping them bit-for-bit compatible means no
 * behavioural change when tools switch from GDK_KEY_* to INK_KEY_*.
 */

// --- Modifier keys ---
constexpr unsigned INK_KEY_Shift_L      = 0xFFE1;
constexpr unsigned INK_KEY_Shift_R      = 0xFFE2;
constexpr unsigned INK_KEY_Control_L    = 0xFFE3;
constexpr unsigned INK_KEY_Control_R    = 0xFFE4;
constexpr unsigned INK_KEY_Alt_L        = 0xFFE9;
constexpr unsigned INK_KEY_Alt_R        = 0xFFEA;
constexpr unsigned INK_KEY_Meta_L       = 0xFFE7;
constexpr unsigned INK_KEY_Meta_R       = 0xFFE8;
constexpr unsigned INK_KEY_Caps_Lock    = 0xFFE5;
constexpr unsigned INK_KEY_Num_Lock     = 0xFF7F;
constexpr unsigned INK_KEY_Scroll_Lock  = 0xFF14;

// --- Special / control keys ---
constexpr unsigned INK_KEY_VoidSymbol   = 0xFFFFFF;
constexpr unsigned INK_KEY_BackSpace    = 0xFF08;
constexpr unsigned INK_KEY_Tab          = 0xFF09;
constexpr unsigned INK_KEY_ISO_Left_Tab = 0xFF15;
constexpr unsigned INK_KEY_ISO_Enter    = 0xFE34;
constexpr unsigned INK_KEY_Return       = 0xFF0D;
constexpr unsigned INK_KEY_Escape       = 0xFF1B;
constexpr unsigned INK_KEY_Delete       = 0xFFFF;
constexpr unsigned INK_KEY_Insert       = 0xFF63;
constexpr unsigned INK_KEY_Pause        = 0xFF13;
constexpr unsigned INK_KEY_Print        = 0xFF61;
constexpr unsigned INK_KEY_Sys_Req      = 0xFF15;
constexpr unsigned INK_KEY_Clear        = 0xFF0B;
constexpr unsigned INK_KEY_Menu         = 0xFF67;
constexpr unsigned INK_KEY_space        = 0x0020;

// --- Navigation keys ---
constexpr unsigned INK_KEY_Home         = 0xFF50;
constexpr unsigned INK_KEY_Left         = 0xFF51;
constexpr unsigned INK_KEY_Up           = 0xFF52;
constexpr unsigned INK_KEY_Right        = 0xFF53;
constexpr unsigned INK_KEY_Down         = 0xFF54;
constexpr unsigned INK_KEY_Page_Up      = 0xFF55;
constexpr unsigned INK_KEY_Page_Down    = 0xFF56;
constexpr unsigned INK_KEY_End          = 0xFF57;

// --- Function keys ---
constexpr unsigned INK_KEY_F1           = 0xFFBE;
constexpr unsigned INK_KEY_F2           = 0xFFBF;
constexpr unsigned INK_KEY_F3           = 0xFFC0;
constexpr unsigned INK_KEY_F4           = 0xFFC1;
constexpr unsigned INK_KEY_F5           = 0xFFC2;
constexpr unsigned INK_KEY_F6           = 0xFFC3;
constexpr unsigned INK_KEY_F7           = 0xFFC4;
constexpr unsigned INK_KEY_F8           = 0xFFC5;
constexpr unsigned INK_KEY_F9           = 0xFFC6;
constexpr unsigned INK_KEY_F10          = 0xFFC7;
constexpr unsigned INK_KEY_F11          = 0xFFC8;
constexpr unsigned INK_KEY_F12          = 0xFFC9;

// --- Keypad keys ---
constexpr unsigned INK_KEY_KP_Space     = 0xFF80;
constexpr unsigned INK_KEY_KP_Tab       = 0xFF89;
constexpr unsigned INK_KEY_KP_Home      = 0xFF95;
constexpr unsigned INK_KEY_KP_Left      = 0xFF96;
constexpr unsigned INK_KEY_KP_Up        = 0xFF97;
constexpr unsigned INK_KEY_KP_Right     = 0xFF98;
constexpr unsigned INK_KEY_KP_Down      = 0xFF99;
constexpr unsigned INK_KEY_KP_Page_Up   = 0xFF9A;
constexpr unsigned INK_KEY_KP_Page_Down = 0xFF9B;
constexpr unsigned INK_KEY_KP_End       = 0xFF9C;
constexpr unsigned INK_KEY_KP_Insert    = 0xFF9E;
constexpr unsigned INK_KEY_KP_Delete    = 0xFF9F;
constexpr unsigned INK_KEY_KP_Enter     = 0xFF8D;
constexpr unsigned INK_KEY_KP_Add       = 0xFFAB;
constexpr unsigned INK_KEY_KP_Subtract  = 0xFFAD;
constexpr unsigned INK_KEY_KP_0         = 0xFFB0;
constexpr unsigned INK_KEY_KP_1         = 0xFFB1;
constexpr unsigned INK_KEY_KP_2         = 0xFFB2;
constexpr unsigned INK_KEY_KP_3         = 0xFFB3;
constexpr unsigned INK_KEY_KP_4         = 0xFFB4;
constexpr unsigned INK_KEY_KP_5         = 0xFFB5;
constexpr unsigned INK_KEY_KP_6         = 0xFFB6;
constexpr unsigned INK_KEY_KP_7         = 0xFFB7;
constexpr unsigned INK_KEY_KP_8         = 0xFFB8;
constexpr unsigned INK_KEY_KP_9         = 0xFFB9;

// --- Printable ASCII (values match ASCII / GDK) ---
constexpr unsigned INK_KEY_0            = 0x0030;
constexpr unsigned INK_KEY_1            = 0x0031;
constexpr unsigned INK_KEY_2            = 0x0032;
constexpr unsigned INK_KEY_3            = 0x0033;
constexpr unsigned INK_KEY_4            = 0x0034;
constexpr unsigned INK_KEY_5            = 0x0035;
constexpr unsigned INK_KEY_6            = 0x0036;
constexpr unsigned INK_KEY_7            = 0x0037;
constexpr unsigned INK_KEY_8            = 0x0038;
constexpr unsigned INK_KEY_9            = 0x0039;

constexpr unsigned INK_KEY_A            = 0x0041;
constexpr unsigned INK_KEY_B            = 0x0042;
constexpr unsigned INK_KEY_C            = 0x0043;
constexpr unsigned INK_KEY_D            = 0x0044;
constexpr unsigned INK_KEY_E            = 0x0045;
constexpr unsigned INK_KEY_F            = 0x0046;
constexpr unsigned INK_KEY_G            = 0x0047;
constexpr unsigned INK_KEY_H            = 0x0048;
constexpr unsigned INK_KEY_I            = 0x0049;
constexpr unsigned INK_KEY_J            = 0x004A;
constexpr unsigned INK_KEY_K            = 0x004B;
constexpr unsigned INK_KEY_L            = 0x004C;
constexpr unsigned INK_KEY_M            = 0x004D;
constexpr unsigned INK_KEY_N            = 0x004E;
constexpr unsigned INK_KEY_O            = 0x004F;
constexpr unsigned INK_KEY_P            = 0x0050;
constexpr unsigned INK_KEY_Q            = 0x0051;
constexpr unsigned INK_KEY_R            = 0x0052;
constexpr unsigned INK_KEY_S            = 0x0053;
constexpr unsigned INK_KEY_T            = 0x0054;
constexpr unsigned INK_KEY_U            = 0x0055;
constexpr unsigned INK_KEY_V            = 0x0056;
constexpr unsigned INK_KEY_W            = 0x0057;
constexpr unsigned INK_KEY_X            = 0x0058;
constexpr unsigned INK_KEY_Y            = 0x0059;
constexpr unsigned INK_KEY_Z            = 0x005A;

constexpr unsigned INK_KEY_a            = 0x0061;
constexpr unsigned INK_KEY_b            = 0x0062;
constexpr unsigned INK_KEY_c            = 0x0063;
constexpr unsigned INK_KEY_d            = 0x0064;
constexpr unsigned INK_KEY_e            = 0x0065;
constexpr unsigned INK_KEY_f            = 0x0066;
constexpr unsigned INK_KEY_g            = 0x0067;
constexpr unsigned INK_KEY_h            = 0x0068;
constexpr unsigned INK_KEY_i            = 0x0069;
constexpr unsigned INK_KEY_j            = 0x006A;
constexpr unsigned INK_KEY_k            = 0x006B;
constexpr unsigned INK_KEY_l            = 0x006C;
constexpr unsigned INK_KEY_m            = 0x006D;
constexpr unsigned INK_KEY_n            = 0x006E;
constexpr unsigned INK_KEY_o            = 0x006F;
constexpr unsigned INK_KEY_p            = 0x0070;
constexpr unsigned INK_KEY_q            = 0x0071;
constexpr unsigned INK_KEY_r            = 0x0072;
constexpr unsigned INK_KEY_s            = 0x0073;
constexpr unsigned INK_KEY_t            = 0x0074;
constexpr unsigned INK_KEY_u            = 0x0075;
constexpr unsigned INK_KEY_v            = 0x0076;
constexpr unsigned INK_KEY_w            = 0x0077;
constexpr unsigned INK_KEY_x            = 0x0078;
constexpr unsigned INK_KEY_y            = 0x0079;
constexpr unsigned INK_KEY_z            = 0x007A;

// --- Punctuation / symbol keys (values match ASCII / GDK) ---
constexpr unsigned INK_KEY_parenleft    = 0x0028;
constexpr unsigned INK_KEY_parenright   = 0x0029;
constexpr unsigned INK_KEY_comma        = 0x002C;
constexpr unsigned INK_KEY_minus        = 0x002D;
constexpr unsigned INK_KEY_period       = 0x002E;
constexpr unsigned INK_KEY_less         = 0x003C;
constexpr unsigned INK_KEY_greater      = 0x003E;
constexpr unsigned INK_KEY_bracketleft  = 0x005B;
constexpr unsigned INK_KEY_bracketright = 0x005D;
constexpr unsigned INK_KEY_braceleft    = 0x007B;
constexpr unsigned INK_KEY_braceright   = 0x007D;

#endif // INKSCAPE_UI_WIDGET_EVENTS_KEYVALS_H
