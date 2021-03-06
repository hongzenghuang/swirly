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
#ifndef SWIRLY_FIN_ORDER_HPP
#define SWIRLY_FIN_ORDER_HPP

#include <swirly/fin/Conv.hpp>
#include <swirly/fin/Request.hpp>

#include <swirly/app/MemAlloc.hpp>

#include <boost/intrusive/list.hpp>

namespace swirly {
inline namespace fin {

class Level;

/**
 * An instruction to buy or sell goods or services.
 */
class SWIRLY_API Order
: public RefCount<Order, ThreadUnsafePolicy>
, public Request
, public MemAlloc {
  public:
    Order(Symbol accnt, Id64 market_id, Symbol instr, JDay settl_day, Id64 id, std::string_view ref,
          State state, Side side, Lots lots, Ticks ticks, Lots resd_lots, Lots exec_lots,
          Cost exec_cost, Lots last_lots, Ticks last_ticks, Lots min_lots, Time created,
          Time modified) noexcept
    : Request{accnt, market_id, instr, settl_day, id, ref, side, lots, created}
    , state_{state}
    , ticks_{ticks}
    , resd_lots_{resd_lots}
    , exec_lots_{exec_lots}
    , exec_cost_{exec_cost}
    , last_lots_{last_lots}
    , last_ticks_{last_ticks}
    , min_lots_{min_lots}
    , modified_{modified}
    {
    }
    Order(Symbol accnt, Id64 market_id, Symbol instr, JDay settl_day, Id64 id, std::string_view ref,
          Side side, Lots lots, Ticks ticks, Lots min_lots, Time created) noexcept
    : Order{accnt, market_id, instr, settl_day, id,    ref,   State::New, side,    lots,
            ticks, lots,      0_lts, 0_cst,     0_lts, 0_tks, min_lots,   created, created}
    {
    }
    ~Order();

    // Copy.
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;

    // Move.
    Order(Order&&);
    Order& operator=(Order&&) = delete;

    template <typename... ArgsT>
    static OrderPtr make(ArgsT&&... args)
    {
        return make_intrusive<Order>(std::forward<ArgsT>(args)...);
    }

    void to_dsv(std::ostream& os, char delim = ',') const;
    void to_json(std::ostream& os) const;

    auto* level() const noexcept { return level_; }
    auto state() const noexcept { return state_; }
    auto ticks() const noexcept { return ticks_; }
    auto resd_lots() const noexcept { return resd_lots_; }
    auto exec_lots() const noexcept { return exec_lots_; }
    auto exec_cost() const noexcept { return exec_cost_; }
    auto last_lots() const noexcept { return last_lots_; }
    auto last_ticks() const noexcept { return last_ticks_; }
    auto min_lots() const noexcept { return min_lots_; }
    auto done() const noexcept { return resd_lots_ == 0_lts; }
    auto modified() const noexcept { return modified_; }
    void set_level(Level* level) const noexcept { level_ = level; }
    void create(Time now) noexcept
    {
        assert(lots_ > 0_lts);
        assert(lots_ >= min_lots_);
        state_ = State::New;
        resd_lots_ = lots_;
        exec_lots_ = 0_lts;
        exec_cost_ = 0_cst;
        modified_ = now;
    }
    void revise(Lots lots, Time now) noexcept
    {
        assert(lots > 0_lts);
        assert(lots >= exec_lots_);
        assert(lots >= min_lots_);
        assert(lots <= lots_);
        const auto delta = lots_ - lots;
        assert(delta >= 0_lts);
        state_ = State::Revise;
        lots_ = lots;
        resd_lots_ -= delta;
        modified_ = now;
    }
    void cancel(Time now) noexcept
    {
        state_ = State::Cancel;
        // Note that executed lots is not affected.
        resd_lots_ = 0_lts;
        modified_ = now;
    }
    void trade(Lots taken_lots, Cost taken_cost, Lots last_lots, Ticks last_ticks,
               Time now) noexcept
    {
        state_ = State::Trade;
        resd_lots_ -= taken_lots;
        exec_lots_ += taken_lots;
        exec_cost_ += taken_cost;
        last_lots_ = last_lots;
        last_ticks_ = last_ticks;
        modified_ = now;
    }
    void trade(Lots last_lots, Ticks last_ticks, Time now) noexcept
    {
        trade(last_lots, swirly::cost(last_lots, last_ticks), last_lots, last_ticks, now);
    }
    boost::intrusive::set_member_hook<> id_hook;
    boost::intrusive::set_member_hook<> ref_hook;
    boost::intrusive::list_member_hook<> list_hook;

  private:
    // Internals.
    mutable Level* level_{nullptr};

    State state_;
    const Ticks ticks_;
    /**
     * Must be greater than zero.
     */
    Lots resd_lots_;
    /**
     * Must not be greater that lots.
     */
    Lots exec_lots_;
    Cost exec_cost_;
    Lots last_lots_;
    Ticks last_ticks_;
    /**
     * Minimum to be filled by this order.
     */
    const Lots min_lots_;
    Time modified_;
};

static_assert(sizeof(Order) <= 5 * 64, "no greater than specified cache-lines");

using OrderIdSet = RequestIdSet<Order>;

class SWIRLY_API OrderRefSet {
    struct ValueCompare {
        int compare(const Order& lhs, const Order& rhs) const noexcept
        {
            return lhs.ref().compare(rhs.ref());
        }
        bool operator()(const Order& lhs, const Order& rhs) const noexcept
        {
            return compare(lhs, rhs) < 0;
        }
    };
    struct KeyValueCompare {
        bool operator()(std::string_view lhs, const Order& rhs) const noexcept
        {
            return lhs.compare(rhs.ref()) < 0;
        }
        bool operator()(const Order& lhs, std::string_view rhs) const noexcept
        {
            return lhs.ref().compare(rhs) < 0;
        }
    };
    using ConstantTimeSizeOption = boost::intrusive::constant_time_size<false>;
    using CompareOption = boost::intrusive::compare<ValueCompare>;
    using MemberHookOption
        = boost::intrusive::member_hook<Order, decltype(Order::ref_hook), &Order::ref_hook>;
    using Set
        = boost::intrusive::set<Order, ConstantTimeSizeOption, CompareOption, MemberHookOption>;
    using ValuePtr = boost::intrusive_ptr<Order>;

  public:
    using Iterator = typename Set::iterator;
    using ConstIterator = typename Set::const_iterator;

    OrderRefSet() = default;

    ~OrderRefSet();

    // Copy.
    OrderRefSet(const OrderRefSet&) = delete;
    OrderRefSet& operator=(const OrderRefSet&) = delete;

    // Move.
    OrderRefSet(OrderRefSet&&);
    OrderRefSet& operator=(OrderRefSet&&);

    // Begin.
    ConstIterator begin() const noexcept { return set_.begin(); }
    ConstIterator cbegin() const noexcept { return set_.cbegin(); }
    Iterator begin() noexcept { return set_.begin(); }

    // End.
    ConstIterator end() const noexcept { return set_.end(); }
    ConstIterator cend() const noexcept { return set_.cend(); }
    Iterator end() noexcept { return set_.end(); }

    // Find.
    ConstIterator find(std::string_view ref) const noexcept
    {
        return set_.find(ref, KeyValueCompare());
    }
    Iterator find(std::string_view ref) noexcept { return set_.find(ref, KeyValueCompare()); }
    std::pair<ConstIterator, bool> find_hint(std::string_view ref) const noexcept
    {
        const auto comp = KeyValueCompare();
        auto it = set_.lower_bound(ref, comp);
        return std::make_pair(it, it != set_.end() && !comp(ref, *it));
    }
    std::pair<Iterator, bool> find_hint(std::string_view ref) noexcept
    {
        const auto comp = KeyValueCompare();
        auto it = set_.lower_bound(ref, comp);
        return std::make_pair(it, it != set_.end() && !comp(ref, *it));
    }
    Iterator insert(const ValuePtr& value) noexcept;

    Iterator insert_hint(ConstIterator hint, const ValuePtr& value) noexcept;

    Iterator insert_or_replace(const ValuePtr& value) noexcept;

    template <typename... ArgsT>
    Iterator emplace(ArgsT&&... args)
    {
        return insert(make_intrusive<Order>(std::forward<ArgsT>(args)...));
    }
    template <typename... ArgsT>
    Iterator emplace_hint(ConstIterator hint, ArgsT&&... args)
    {
        return insert_hint(hint, make_intrusive<Order>(std::forward<ArgsT>(args)...));
    }
    template <typename... ArgsT>
    Iterator emplace_or_replace(ArgsT&&... args)
    {
        return insert_or_replace(make_intrusive<Order>(std::forward<ArgsT>(args)...));
    }
    ValuePtr remove(const Order& ref) noexcept
    {
        ValuePtr value;
        set_.erase_and_dispose(ref, [&value](Order* ptr) { value = ValuePtr{ptr, false}; });
        return value;
    }

  private:
    Set set_;
};

class SWIRLY_API OrderList {
    using ConstantTimeSizeOption = boost::intrusive::constant_time_size<false>;
    using MemberHookOption
        = boost::intrusive::member_hook<Order, decltype(Order::list_hook), &Order::list_hook>;
    using List = boost::intrusive::list<Order, ConstantTimeSizeOption, MemberHookOption>;
    using ValuePtr = boost::intrusive_ptr<Order>;

  public:
    using Iterator = typename List::iterator;
    using ConstIterator = typename List::const_iterator;

    OrderList() = default;

    ~OrderList();

    // Copy.
    OrderList(const OrderList&) = delete;
    OrderList& operator=(const OrderList&) = delete;

    // Move.
    OrderList(OrderList&&);
    OrderList& operator=(OrderList&&);

    static ConstIterator to_iterator(const Order& order) noexcept
    {
        return List::s_iterator_to(order);
    }

    // Begin.
    ConstIterator begin() const noexcept { return list_.begin(); }
    ConstIterator cbegin() const noexcept { return list_.cbegin(); }
    Iterator begin() noexcept { return list_.begin(); }

    // End.
    ConstIterator end() const noexcept { return list_.end(); }
    ConstIterator cend() const noexcept { return list_.cend(); }
    Iterator end() noexcept { return list_.end(); }
    Iterator insert_back(const OrderPtr& value) noexcept;

    Iterator insert_before(const OrderPtr& value, const Order& next) noexcept;

    ValuePtr remove(const Order& ref) noexcept;

  private:
    List list_;
};

inline std::ostream& operator<<(std::ostream& os, const Order& order)
{
    order.to_json(os);
    return os;
}

} // namespace fin
} // namespace swirly

#endif // SWIRLY_FIN_ORDER_HPP
