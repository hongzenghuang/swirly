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
#ifndef DBR_TRADER_HPP
#define DBR_TRADER_HPP

#include <dbr/iter.hpp>
#include <dbr/node.hpp>

#include <dbr/conv.h>
#include <dbr/trader.h>

#include <iostream>
#include <limits>

namespace dbr {

class TraderOrders {
    struct Policy : NodeTraits<DbrRbNode> {
        typedef DbrOrder Entry;
        static Entry*
        entry(Node* node)
        {
            return dbr_trader_order_entry(node);
        }
        static const Entry*
        entry(const Node* node)
        {
            return dbr_trader_order_entry(const_cast<Node*>(node));
        }
    };
    DbrTrader trader_;
public:
    typedef Policy::Entry ValueType;
    typedef Policy::Entry* Pointer;
    typedef Policy::Entry& Reference;
    typedef const Policy::Entry* ConstPointer;
    typedef const Policy::Entry& ConstReference;

    typedef BiDirectionalIterator<Policy> Iterator;
    typedef ConstBiDirectionalIterator<Policy> ConstIterator;
    typedef ReverseBiDirectionalIterator<Policy> ReverseIterator;
    typedef ConstReverseBiDirectionalIterator<Policy> ConstReverseIterator;

    typedef std::ptrdiff_t DifferenceType;
    typedef size_t SizeType;

    // Standard typedefs.

    typedef ValueType value_type;
    typedef Pointer pointer;
    typedef Reference reference;
    typedef ConstPointer const_pointer;
    typedef ConstReference const_reference;

    typedef Iterator iterator;
    typedef ConstIterator const_iterator;
    typedef ReverseIterator reverse_iterator;
    typedef ConstReverseIterator const_reverse_iterator;

    typedef DifferenceType difference_type;
    typedef DifferenceType distance_type;
    typedef SizeType size_type;

    explicit
    TraderOrders(DbrTrader trader) noexcept
    : trader_(trader)
    {
    }
    void
    swap(TraderOrders& rhs) noexcept
    {
        std::swap(trader_, rhs.trader_);
    }

    // Iterator.

    Iterator
    begin() noexcept
    {
        return dbr_trader_first_order(trader_);
    }
    ConstIterator
    begin() const noexcept
    {
        return dbr_trader_first_order(trader_);
    }
    Iterator
    end() noexcept
    {
        return dbr_trader_end_order(trader_);
    }
    ConstIterator
    end() const noexcept
    {
        return dbr_trader_end_order(trader_);
    }

    // ReverseIterator.

    ReverseIterator
    rbegin() noexcept
    {
        return dbr_trader_last_order(trader_);
    }
    ConstReverseIterator
    rbegin() const noexcept
    {
        return dbr_trader_last_order(trader_);
    }
    ReverseIterator
    rend() noexcept
    {
        return dbr_trader_end_order(trader_);
    }
    ConstReverseIterator
    rend() const noexcept
    {
        return dbr_trader_end_order(trader_);
    }

    // Find.

    Iterator
    find(DbrTicks ticks) noexcept
    {
        return dbr_trader_find_order_id(trader_, ticks);
    }
    ConstIterator
    find(DbrTicks ticks) const noexcept
    {
        return dbr_trader_find_order_id(trader_, ticks);
    }
    Iterator
    find(const char* ref) noexcept
    {
        DbrOrder* const order = dbr_trader_find_order_ref(trader_, ref);
        return order ? &order->trader_node_ : end();
    }
    ConstIterator
    find(const char* ref) const noexcept
    {
        DbrOrder* const order = dbr_trader_find_order_ref(trader_, ref);
        return order ? &order->trader_node_ : end();
    }

    // Accessor.

    Reference
    front() noexcept
    {
        return *begin();
    }
    ConstReference
    front() const noexcept
    {
        return *begin();
    }
    Reference
    back() noexcept
    {
        return *rbegin();
    }
    ConstReference
    back() const noexcept
    {
        return *rbegin();
    }
    SizeType
    size() const noexcept
    {
        return std::distance(begin(), end());
    }
    SizeType
    max_size() const noexcept
    {
        return std::numeric_limits<SizeType>::max();
    }
    bool
    empty() const noexcept
    {
        return dbr_trader_empty_order(trader_);
    }
};

class Trader {
    DbrTrader impl_;
public:
    explicit
    Trader(DbrTrader impl) noexcept
        : impl_(impl)
    {
    }
    explicit
    operator DbrTrader() const noexcept
    {
        return impl_;
    }
    bool
    operator ==(Trader rhs) const noexcept
    {
        return id() == rhs.id();
    }
    bool
    operator !=(Trader rhs) const noexcept
    {
        return id() != rhs.id();
    }
    DbrIden
    id() const noexcept
    {
        return dbr_trader_id(impl_);
    }
    TraderOrders
    orders() const noexcept
    {
        return TraderOrders(impl_);
    }
};

inline std::ostream&
operator <<(std::ostream& os, Trader trader)
{
    return os << "id=" << trader.id();
}

} // dbr

#endif // DBR_TRADER_HPP