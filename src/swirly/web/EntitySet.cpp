/*
 * The Restful Matching-Engine.
 * Copyright (C) 2013, 2018 Swirly Cloud Limited.
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
#include "EntitySet.hpp"

#include <swirly/fin/Exception.hpp>

#include <swirly/util/Tokeniser.hpp>

namespace swirly {
inline namespace web {
using namespace std;
namespace {

int bit(string_view sv)
{
    // Support both plural and singular forms.
    if (!sv.empty() && sv.back() == 's') {
        sv.remove_suffix(1);
    }
    int i;
    if (sv == "asset") {
        i = EntitySet::Asset;
    } else if (sv == "instr") {
        i = EntitySet::Instr;
    } else if (sv == "market") {
        i = EntitySet::Market;
    } else if (sv == "order") {
        i = EntitySet::Order;
    } else if (sv == "exec") {
        i = EntitySet::Exec;
    } else if (sv == "trade") {
        i = EntitySet::Trade;
    } else if (sv == "posn") {
        i = EntitySet::Posn;
    } else {
        throw NotFoundException{err_msg() << "unknown entity '" << sv << '\''};
    }
    return i;
}

} // namespace

EntitySet::~EntitySet() = default;

EntitySet EntitySet::parse(string_view sv)
{
    int bs{0};
    Tokeniser toks{sv, ","sv};
    while (!toks.empty()) {
        bs |= bit(toks.top());
        toks.pop();
    }
    return {bs};
}

} // namespace web
} // namespace swirly
