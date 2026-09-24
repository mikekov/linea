// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef LINEA_SCRIPT_ERROR_H
#define LINEA_SCRIPT_ERROR_H

#include <string>

namespace Linea::Script {

enum class ErrorKind {
    Parse,
    Runtime,
    Cancelled,
};

struct Error {
    ErrorKind kind = ErrorKind::Runtime;
    std::string message;
    int line = 0;
    int column = 0;
};

} // namespace Linea::Script

#endif // LINEA_SCRIPT_ERROR_H
