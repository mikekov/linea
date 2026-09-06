#ifndef LINEA_ACTION_STRINGS_H
#define LINEA_ACTION_STRINGS_H

#include <QtGlobal>

#undef N_
#undef NC_

// QT_TR_NOOP marks strings for translation without runtime translation
// lupdate extracts these for .ts files
#define N_(x) QT_TR_NOOP(x)

#define NC_(c, x) QT_TR_NOOP(x)

#endif
