// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * File open errors.
 */

#ifndef LINEA_IO_FILE_ERROR_H
#define LINEA_IO_FILE_ERROR_H

#include <optional>
#include <string>
#include <QString>

namespace Linea::IO {

enum class OpenFailure {
    FileNotFound,
    OpenError,
    ExtensionNotFound,
    ReadError,
    ParseError,
    NotSvg,
    UnsupportedFormat,
    Cancelled
};

struct FileError {
    FileError() = default;
    FileError(OpenFailure reason, std::string message = {})
        : reason(reason)
        , message(std::move(message)) {}

    std::optional<OpenFailure> reason;
    std::string message;

    bool cancelled() const { return reason && *reason == OpenFailure::Cancelled; }
    QString error() const;
};

} // namespace Linea::IO

#endif // LINEA_IO_FILE_ERROR_H
