/*
 *  Copyright (C) 2013, 2014 Mark Aylett <mark.aylett@gmail.com>
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
#include "trader.h"

#include <dbr/book.h>
#include <dbr/err.h>

#include <stdlib.h>
#include <string.h>

static void
free_orders(struct FigTrader* trader)
{
    assert(trader);
    struct DbrRbNode* node;
    while ((node = trader->orders.root)) {
        struct DbrOrder* order = dbr_trader_order_entry(node);
        fig_trader_release_order(trader, order);
        dbr_pool_free_order(trader->pool, order);
    }
}

static void
free_execs(struct FigTrader* trader)
{
    assert(trader);
    struct DbrRbNode* node;
    while ((node = trader->trades.root)) {
        struct DbrExec* exec = dbr_trader_trade_entry(node);
        dbr_tree_remove(&trader->trades, node);
        dbr_exec_decref(exec, trader->pool);
    }
}

static void
free_membs(struct FigTrader* trader)
{
    assert(trader);
    struct DbrRbNode* node;
    while ((node = trader->membs.root)) {
        struct DbrMemb* memb = dbr_trader_memb_entry(node);
        dbr_tree_remove(&trader->membs, node);
        dbr_pool_free_memb(trader->pool, memb);
    }
}

DBR_EXTERN struct FigTrader*
fig_trader_lazy(struct DbrRec* trec, struct FigOrdIdx* ordidx, DbrPool pool)
{
    assert(trec);
    assert(trec->type == DBR_ENTITY_TRADER);
    struct FigTrader* trader = trec->trader.state;
    if (dbr_unlikely(!trader)) {
        trader = malloc(sizeof(struct FigTrader));
        if (dbr_unlikely(!trader)) {
            dbr_err_set(DBR_ENOMEM, "out of memory");
            return NULL;
        }
        trader->rec = trec;
        trader->ordidx = ordidx;
        trader->pool = pool;
        dbr_tree_init(&trader->orders);
        dbr_tree_init(&trader->trades);
        dbr_tree_init(&trader->membs);
        trader->sess = NULL;
        dbr_rbnode_init(&trader->sess_node_);

        trec->trader.state = trader;
    }
    return trader;
}

DBR_EXTERN void
fig_trader_term(struct DbrRec* trec)
{
    assert(trec);
    assert(trec->type == DBR_ENTITY_TRADER);
    struct FigTrader* trader = trec->trader.state;
    if (trader) {
        trec->trader.state = NULL;
        free_membs(trader);
        free_execs(trader);
        free_orders(trader);
        free(trader);
    }
}

DBR_API struct DbrRec*
dbr_trader_rec(DbrTrader trader)
{
    return fig_trader_rec(trader);
}

// TraderOrder

DBR_API struct DbrRbNode*
dbr_trader_find_order_id(DbrTrader trader, DbrIden id)
{
    return fig_trader_find_order_id(trader, id);
}

DBR_API struct DbrOrder*
dbr_trader_find_order_ref(DbrTrader trader, const char* ref)
{
    return fig_trader_find_order_ref(trader, ref);
}

DBR_API struct DbrRbNode*
dbr_trader_first_order(DbrTrader trader)
{
    return fig_trader_first_order(trader);
}

DBR_API struct DbrRbNode*
dbr_trader_last_order(DbrTrader trader)
{
    return fig_trader_last_order(trader);
}

DBR_API DbrBool
dbr_trader_empty_order(DbrTrader trader)
{
    return fig_trader_empty_order(trader);
}

// TraderTrade

DBR_API struct DbrRbNode*
dbr_trader_find_trade_id(DbrTrader trader, DbrIden id)
{
    return fig_trader_find_trade_id(trader, id);
}

DBR_API struct DbrRbNode*
dbr_trader_first_trade(DbrTrader trader)
{
    return fig_trader_first_trade(trader);
}

DBR_API struct DbrRbNode*
dbr_trader_last_trade(DbrTrader trader)
{
    return fig_trader_last_trade(trader);
}

DBR_API DbrBool
dbr_trader_empty_trade(DbrTrader trader)
{
    return fig_trader_empty_trade(trader);
}

// TraderMemb

DBR_API struct DbrRbNode*
dbr_trader_find_memb_id(DbrTrader trader, DbrIden id)
{
    return fig_trader_find_memb_id(trader, id);
}

DBR_API struct DbrRbNode*
dbr_trader_first_memb(DbrTrader trader)
{
    return fig_trader_first_memb(trader);
}

DBR_API struct DbrRbNode*
dbr_trader_last_memb(DbrTrader trader)
{
    return fig_trader_last_memb(trader);
}

DBR_API DbrBool
dbr_trader_empty_memb(DbrTrader trader)
{
    return fig_trader_empty_memb(trader);
}
