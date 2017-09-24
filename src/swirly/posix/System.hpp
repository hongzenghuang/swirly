/*
 * The Restful Matching-Engine.
 * Copyright (C) 2013, 2017 Swirly Cloud Limited.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if
 * not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef SWIRLY_POSIX_SYSTEM_HPP
#define SWIRLY_POSIX_SYSTEM_HPP

#include <swirly/posix/Util.hpp>

#include <unistd.h>

namespace swirly {
namespace posix {

/**
 * Change working directory.
 */
inline void chdir(const char* path, std::error_code& ec) noexcept
{
    const auto ret = ::chdir(path);
    if (ret < 0) {
        ec = error(errno);
    }
}

/**
 * Change working directory.
 */
inline void chdir(const char* path)
{
    const auto ret = ::chdir(path);
    if (ret < 0) {
        throw std::system_error{error(errno), "chdir"};
    }
}

/**
 * Create a child process.
 */
inline pid_t fork(std::error_code& ec) noexcept
{
    const auto pid = ::fork();
    if (pid < 0) {
        ec = error(errno);
    }
    return pid;
}

/**
 * Create a child process.
 */
inline pid_t fork()
{
    const auto pid = ::fork();
    if (pid < 0) {
        throw std::system_error{error(errno), "fork"};
    }
    return pid;
}

/**
 * Creates a session and sets the process group ID.
 */
inline pid_t setsid(std::error_code& ec) noexcept
{
    const auto sid = ::setsid();
    if (sid < 0) {
        ec = error(errno);
    }
    return sid;
}

/**
 * Creates a session and sets the process group ID.
 */
inline pid_t setsid()
{
    const auto sid = ::setsid();
    if (sid < 0) {
        throw std::system_error{error(errno), "setsid"};
    }
    return sid;
}

} // namespace posix
} // namespace swirly

#endif // SWIRLY_POSIX_SYSTEM_HPP
