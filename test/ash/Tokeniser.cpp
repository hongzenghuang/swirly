/*
 * The Restful Matching-Engine.
 * Copyright (C) 2013, 2016 Swirly Cloud Limited.
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
#include <swirly/ash/Tokeniser.hpp>

#include <test/Test.hpp>

using namespace swirly;

SWIRLY_TEST_CASE(TokeniserEmpty)
{
  Tokeniser<','> toks;

  SWIRLY_CHECK(toks.empty());

  toks = Tokeniser<','>{""_sv};
  SWIRLY_CHECK(toks.empty());
}

SWIRLY_TEST_CASE(TokeniserDelim)
{
  Tokeniser<','> toks{","_sv};

  SWIRLY_CHECK(toks.top().empty());

  toks.pop();
  SWIRLY_CHECK(toks.empty());
}

SWIRLY_TEST_CASE(TokeniserSingle)
{
  Tokeniser<','> toks{"foo"_sv};

  SWIRLY_CHECK(toks.top() == "foo");

  toks.pop();
  SWIRLY_CHECK(toks.empty());
}

SWIRLY_TEST_CASE(TokeniserMany)
{
  Tokeniser<','> toks{"foo,bar,baz"_sv};

  SWIRLY_CHECK(toks.top() == "foo");

  toks.pop();
  SWIRLY_CHECK(toks.top() == "bar");

  toks.pop();
  SWIRLY_CHECK(toks.top() == "baz");

  toks.pop();
  SWIRLY_CHECK(toks.empty());
}

SWIRLY_TEST_CASE(TokeniserLeading)
{
  Tokeniser<','> toks{",foo"_sv};

  SWIRLY_CHECK(toks.top().empty());

  toks.pop();
  SWIRLY_CHECK(toks.top() == "foo");

  toks.pop();
  SWIRLY_CHECK(toks.empty());
}

SWIRLY_TEST_CASE(TokeniserTrailing)
{
  Tokeniser<','> toks{"foo,"_sv};

  SWIRLY_CHECK(toks.top() == "foo");

  toks.pop();
  SWIRLY_CHECK(toks.empty());
}
