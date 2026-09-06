// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * File open errors.
 */

#include "file-error.h"

#include <QObject>

namespace Linea::IO {

QString FileError::error() const {
    if (!reason) {
        return QString::fromStdString(message);
    }
    
    switch (*reason) {
        case OpenFailure::FileNotFound:
            return QObject::tr("File not found");
        case OpenFailure::OpenError:
            return QObject::tr("Open error");
        case OpenFailure::ExtensionNotFound:
            return QObject::tr("Extension not found");
        case OpenFailure::ReadError:
            return QObject::tr("Read error");
        case OpenFailure::ParseError:
            return QObject::tr("Parse error");
        case OpenFailure::NotSvg:
            return QObject::tr("Not SVG");
        case OpenFailure::UnsupportedFormat:
            return QObject::tr("Unsupported format");
        case OpenFailure::Cancelled:
            return QObject::tr("Cancelled");
    }
}

} // namespace Linea::IO
