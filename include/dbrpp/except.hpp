/*
 *  Copyright (C) 2013 Mark Aylett <mark.aylett@gmail.com>
 *
 *  This file is part of Doobry written by Mark Aylett.
 *
 *  Doobry is free software; you can redistribute it and/or modify it under the terms of the GNU
 *  General Public License as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Doobry is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 *  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program; if
 *  not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301 USA.
 */
#ifndef DBRPP_EXCEPT_HPP
#define DBRPP_EXCEPT_HPP

#include <dbr/err.h>

#include <cstring>
#include <stdexcept>

namespace dbr {

class DbrException : public std::exception {
    int num_;
    const char* file_;
    int line_;
    char msg_[DBR_ERROR_MAX + 1];
public:
    virtual
    ~DbrException() noexcept
    {
    }
    DbrException(int num, const char* file, int line, const char* msg) noexcept
    {
        num_ = num;
        file_ = file;
        line_ = line;
        strncpy(msg_, msg, DBR_ERROR_MAX);
        msg_[DBR_ERROR_MAX] = '\0';
    }
    DbrException() noexcept
    : DbrException(dbr_err_num(), dbr_err_file(), dbr_err_line(), dbr_err_msg())
    {
    }
    virtual const char*
    what() const noexcept
    {
        return msg_;
    }
    int
    num() const noexcept
    {
        return num_;
    }
    const char*
    file() const noexcept
    {
        return file_;
    }
    int
    line() const noexcept
    {
        return line_;
    }
};

template <int Num>
class BasicException : public DbrException {
public:
    virtual
    ~BasicException() noexcept
    {
    }
    BasicException(const char* file, int line, const char* msg) noexcept
    : DbrException(Num, file, line, msg)
    {
    }
};

typedef BasicException<DBR_EIO> IoException;
typedef BasicException<DBR_ENOMEM> NoMemException;
typedef BasicException<DBR_EACCES> AccesException;
typedef BasicException<DBR_EINVAL> InvalException;
typedef BasicException<DBR_ENULL>  NullException;
typedef BasicException<DBR_EASSRT> AssrtException;

inline void
throw_exception(int num, const char* file, int line, const char* msg)
{
    switch (num) {
    case DBR_EIO:
        throw IoException(file, line, msg);
        break;
    case DBR_ENOMEM:
        throw NoMemException(file, line, msg);
        break;
    case DBR_EACCES:
        throw AccesException(file, line, msg);
        break;
    case DBR_EINVAL:
        throw InvalException(file, line, msg);
        break;
    case DBR_ENULL:
        throw NullException(file, line, msg);
        break;
    case DBR_EASSRT:
        throw AssrtException(file, line, msg);
        break;
    default:
        throw DbrException(num, file, line, msg);
        break;
    }
}

inline void
throw_exception()
{
    throw_exception(dbr_err_num(), dbr_err_file(), dbr_err_line(), dbr_err_msg());
}
} // dbr

#endif // DBRPP_EXCEPT_HPP