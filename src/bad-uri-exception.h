// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_BAD_URI_EXCEPTION_H
#define SEEN_BAD_URI_EXCEPTION_H

#include <exception>

namespace Inkscape {

class BadURIException : public std::exception {};

class UnsupportedURIException : public BadURIException {
public:
    char const *what() const noexcept override { return "Unsupported URI"; }
};

class MalformedURIException : public BadURIException {
public:
    char const *what() const noexcept override { return "Malformed URI"; }
};

}


#endif /* !SEEN_BAD_URI_EXCEPTION_H */
