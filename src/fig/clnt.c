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
#define _XOPEN_SOURCE 700 // strnlen()

#include "accnt.h"
#include "cache.h"
#include "trader.h"

#include <dbr/clnt.h>
#include <dbr/conv.h> // dbr_book_key()
#include <dbr/err.h>
#include <dbr/msg.h>
#include <dbr/prioq.h>
#include <dbr/queue.h>
#include <dbr/util.h>

#include <zmq.h>

#include <stdbool.h>
#include <stdlib.h> // malloc()
#include <string.h> // strncpy()

enum { TIMEOUT = 5000 };

struct FigClnt {
    void* ctx;
    void* sub;
    void* dealer;
    DbrMnem mnem;
    DbrIden id;
    DbrPool pool;
    int pending;
    DbrTrader trader;
    struct FigCache cache;
    struct FigIndex index;
    struct DbrQueue execs;
    struct DbrTree views;
    struct DbrTree posnups;
    struct DbrTree viewups;
    struct DbrPrioq prioq;
};

static void
term_state(struct DbrRec* rec)
{
    switch (rec->type) {
    case DBR_TRADER:
        fig_trader_term(rec);
        break;
    case DBR_ACCNT:
        fig_accnt_term(rec);
        break;
    }
}

static inline struct DbrRec*
get_id(struct FigCache* cache, int type, DbrIden id)
{
    struct DbrSlNode* node = fig_cache_find_rec_id(cache, type, id);
    assert(node != FIG_CACHE_END_REC);
    return dbr_shared_rec_entry(node);
}

static inline struct DbrOrder*
enrich_order(struct FigCache* cache, struct DbrOrder* order)
{
    order->c.trader.rec = get_id(cache, DBR_TRADER, order->c.trader.id_only);
    order->c.accnt.rec = get_id(cache, DBR_ACCNT, order->c.accnt.id_only);
    order->c.contr.rec = get_id(cache, DBR_CONTR, order->c.contr.id_only);
    return order;
}

static inline struct DbrExec*
enrich_exec(struct FigCache* cache, struct DbrExec* exec)
{
    exec->c.trader.rec = get_id(cache, DBR_TRADER, exec->c.trader.id_only);
    exec->c.accnt.rec = get_id(cache, DBR_ACCNT, exec->c.accnt.id_only);
    exec->c.contr.rec = get_id(cache, DBR_CONTR, exec->c.contr.id_only);
    if (exec->cpty.id_only)
        exec->cpty.rec = get_id(cache, DBR_ACCNT, exec->cpty.id_only);
    else
        exec->cpty.rec = NULL;
    return exec;
}

static inline struct DbrMemb*
enrich_memb(struct FigCache* cache, struct DbrMemb* memb)
{
    memb->trader.rec = get_id(cache, DBR_TRADER, memb->trader.id_only);
    memb->accnt.rec = get_id(cache, DBR_ACCNT, memb->accnt.id_only);
    return memb;
}

static inline struct DbrPosn*
enrich_posn(struct FigCache* cache, struct DbrPosn* posn)
{
    posn->accnt.rec = get_id(cache, DBR_ACCNT, posn->accnt.id_only);
    posn->contr.rec = get_id(cache, DBR_CONTR, posn->contr.id_only);
    return posn;
}

static inline struct DbrView*
enrich_view(struct FigCache* cache, struct DbrView* view)
{
    view->contr.rec = get_id(cache, DBR_CONTR, view->contr.id_only);
    return view;
}

static void
free_views(struct DbrTree* views, DbrPool pool)
{
    assert(views);
    struct DbrRbNode* node;
    while ((node = views->root)) {
        struct DbrView* view = dbr_clnt_view_entry(node);
        dbr_tree_remove(views, node);
        dbr_pool_free_view(pool, view);
    }
}

static inline void
insert_posnup(struct DbrTree* posnups, struct DbrPosn* posn)
{
    dbr_tree_insert(posnups, (DbrKey)posn, &posn->update_node_);
}

static inline void
insert_viewup(struct DbrTree* viewups, struct DbrView* view)
{
    dbr_tree_insert(viewups, (DbrKey)view, &view->update_node_);
}

static DbrIden
logon(DbrClnt clnt)
{
    struct DbrBody body = { .req_id = clnt->id++, body.type = DBR_SESS_LOGON };
    if (!dbr_send_body(clnt->dealer, &body, false))
        return -1;
    return body.req_id;
}

static inline DbrBool
set_trader(DbrClnt clnt)
{
    struct DbrSlNode* node = fig_cache_find_rec_mnem(&clnt->cache, DBR_TRADER, clnt->mnem);
    if (node == FIG_CACHE_END_REC)
        goto fail1;

    struct DbrRec* trec = dbr_shared_rec_entry(node);
    DbrTrader trader = fig_trader_lazy(trec, &clnt->index, clnt->pool);
    if (!trader)
        goto fail1;

    clnt->trader = trader;
    return true;
 fail1:
    return false;
}

static void
emplace_rec_list(DbrClnt clnt, int type, struct DbrSlNode* first, size_t count)
{
    fig_cache_emplace_rec_list(&clnt->cache, type, first, count);
    clnt->pending &= ~type;
    if ((clnt->pending & (DBR_TRADER | DBR_ACCNT | DBR_CONTR)) == 0) {
        if (!set_trader(clnt))
            abort();
    }
}

static void
emplace_order_list(DbrClnt clnt, struct DbrSlNode* first)
{
    for (struct DbrSlNode* node = first; node; node = node->next) {
        struct DbrOrder* order = enrich_order(&clnt->cache, dbr_shared_order_entry(node));
        // Transfer ownership.
        fig_trader_emplace_order(clnt->trader, order);
    }
    clnt->pending &= ~DBR_ORDER;
}

static void
emplace_exec_list(DbrClnt clnt, struct DbrSlNode* first)
{
    for (struct DbrSlNode* node = first; node; node = node->next) {
        struct DbrExec* exec = enrich_exec(&clnt->cache, dbr_shared_exec_entry(node));
        assert(exec->c.state == DBR_TRADE);
        // Transfer ownership.
        fig_trader_emplace_trade(clnt->trader, exec);
    }
    clnt->pending &= ~DBR_EXEC;
}

static void
emplace_memb_list(DbrClnt clnt, struct DbrSlNode* first)
{
    for (struct DbrSlNode* node = first; node; node = node->next) {
        struct DbrMemb* memb = enrich_memb(&clnt->cache, dbr_shared_memb_entry(node));
        // Transfer ownership.
        fig_trader_emplace_memb(clnt->trader, memb);
        DbrAccnt accnt = fig_accnt_lazy(memb->accnt.rec, clnt->pool);
        if (!accnt)
            abort();
    }
    clnt->pending &= ~DBR_MEMB;
}

static void
emplace_posn_list(DbrClnt clnt, struct DbrSlNode* first)
{
    for (struct DbrSlNode* node = first; node; node = node->next) {
        struct DbrPosn* posn = enrich_posn(&clnt->cache, dbr_shared_posn_entry(node));
        // Transfer ownership.
        // All accnts that trader is member of are created in emplace_membs().
        DbrAccnt accnt = posn->accnt.rec->accnt.state;
        assert(accnt);
        fig_accnt_emplace_posn(accnt, posn);
    }
    clnt->pending &= ~DBR_POSN;
}

static void
emplace_view_list(DbrClnt clnt, struct DbrSlNode* first)
{
    for (struct DbrSlNode* node = first; node; node = node->next) {
        struct DbrView* view = enrich_view(&clnt->cache, dbr_shared_view_entry(node));
        dbr_tree_insert(&clnt->views, dbr_book_key(view->contr.rec->id, view->settl_date),
                        &view->clnt_node_);
    }
    clnt->pending &= ~DBR_VIEW;
}

static struct DbrOrder*
create_order(DbrClnt clnt, struct DbrExec* exec)
{
    struct DbrOrder* order = dbr_pool_alloc_order(clnt->pool);
    if (!order)
        return NULL;

    dbr_order_init(order);
    order->level = NULL;
    order->id = exec->order;
    __builtin_memcpy(&order->c, &exec->c, sizeof(struct DbrCommon));
    order->created = exec->created;
    order->modified = exec->created;
    return order;
}

static DbrBool
apply_new(DbrClnt clnt, struct DbrExec* exec)
{
    enrich_exec(&clnt->cache, exec);
    struct DbrOrder* order = create_order(clnt, exec);
    if (!order) {
        dbr_pool_free_exec(clnt->pool, exec);
        return false;
    }
    // Transfer ownership.
    fig_trader_emplace_order(clnt->trader, order);
    dbr_queue_insert_back(&clnt->execs, &exec->shared_node_);
    return true;
}

static DbrBool
apply_update(DbrClnt clnt, struct DbrExec* exec)
{
    enrich_exec(&clnt->cache, exec);
    struct DbrRbNode* node = fig_trader_find_order_id(clnt->trader, exec->order);
    if (!node) {
        dbr_pool_free_exec(clnt->pool, exec);
        dbr_err_setf(DBR_EINVAL, "no such order '%ld'", exec->order);
        return false;
    }

    struct DbrOrder* order = dbr_trader_order_entry(node);
    order->c.state = exec->c.state;
    order->c.lots = exec->c.lots;
    order->c.resd = exec->c.resd;
    order->c.exec = exec->c.exec;
    order->c.last_ticks = exec->c.last_ticks;
    order->c.last_lots = exec->c.last_lots;
    order->modified = exec->created;

    if (exec->c.state == DBR_TRADE) {
        // Transfer ownership.
        fig_trader_emplace_trade(clnt->trader, exec);
    }
    dbr_queue_insert_back(&clnt->execs, &exec->shared_node_);
    return true;
}

static void
apply_posnup(DbrClnt clnt, struct DbrPosn* posn)
{
    enrich_posn(&clnt->cache, posn);
    posn = fig_accnt_update_posn(posn->accnt.rec->accnt.state, posn);
    insert_posnup(&clnt->posnups, posn);
}

static void
apply_viewup(DbrClnt clnt, struct DbrView* view)
{
    enrich_view(&clnt->cache, view);
    const DbrIden key = dbr_book_key(view->contr.rec->id, view->settl_date);
    struct DbrRbNode* node = dbr_tree_insert(&clnt->views, key, &view->clnt_node_);
    if (node != &view->clnt_node_) {
        struct DbrView* curr = dbr_clnt_view_entry(node);

        // Update existing position.

        assert(curr->contr.rec == view->contr.rec);
        assert(curr->settl_date == view->settl_date);

        curr->bid_ticks = view->bid_ticks;
        curr->bid_lots = view->bid_lots;
        curr->bid_count = view->bid_count;
        curr->ask_ticks = view->ask_ticks;
        curr->ask_lots = view->ask_lots;
        curr->ask_count = view->ask_count;

        dbr_pool_free_view(clnt->pool, view);
        view = curr;
    }
    insert_viewup(&clnt->viewups, view);
}

DBR_API DbrClnt
dbr_clnt_create(void* ctx, const char* sub_addr, const char* dealer_addr, const char* trader,
                DbrIden seed, DbrPool pool)
{
    DbrClnt clnt = malloc(sizeof(struct FigClnt));
    if (dbr_unlikely(!clnt)) {
        dbr_err_set(DBR_ENOMEM, "out of memory");
        goto fail1;
    }

    void* sub = zmq_socket(ctx, ZMQ_SUB);
    if (!sub) {
        dbr_err_setf(DBR_EIO, "zmq_socket() failed: %s", zmq_strerror(zmq_errno()));
        goto fail2;
    }
    zmq_setsockopt(sub, ZMQ_SUBSCRIBE, "", 0);

    if (zmq_connect(sub, sub_addr) < 0) {
        dbr_err_setf(DBR_EIO, "zmq_connect() failed: %s", zmq_strerror(zmq_errno()));
        goto fail3;
    }

    void* dealer = zmq_socket(ctx, ZMQ_DEALER);
    if (!dealer) {
        dbr_err_setf(DBR_EIO, "zmq_socket() failed: %s", zmq_strerror(zmq_errno()));
        goto fail3;
    }
    zmq_setsockopt(dealer, ZMQ_IDENTITY, trader, strnlen(trader, DBR_MNEM_MAX));

    if (zmq_connect(dealer, dealer_addr) < 0) {
        dbr_err_setf(DBR_EIO, "zmq_connect() failed: %s", zmq_strerror(zmq_errno()));
        goto fail4;
    }

    clnt->sub = sub;
    clnt->dealer = dealer;
    strncpy(clnt->mnem, trader, DBR_MNEM_MAX);
    clnt->id = seed;
    clnt->pool = pool;
    clnt->pending = DBR_TRADER | DBR_ACCNT | DBR_CONTR | DBR_ORDER | DBR_EXEC | DBR_MEMB
        | DBR_POSN | DBR_VIEW;
    clnt->trader = NULL;
    fig_cache_init(&clnt->cache, term_state, pool);
    fig_index_init(&clnt->index);
    dbr_queue_init(&clnt->execs);
    dbr_tree_init(&clnt->views);
    dbr_tree_init(&clnt->posnups);
    dbr_tree_init(&clnt->viewups);
    if (!dbr_prioq_init(&clnt->prioq))
        goto fail5;
    if (!logon(clnt))
        goto fail6;
    return clnt;
 fail6:
    dbr_prioq_term(&clnt->prioq);
 fail5:
    fig_cache_term(&clnt->cache);
 fail4:
    zmq_close(dealer);
 fail3:
    zmq_close(sub);
 fail2:
    free(clnt);
 fail1:
    return NULL;
}

DBR_API void
dbr_clnt_destroy(DbrClnt clnt)
{
    if (clnt) {
        // Ensure that executions are freed.
        dbr_clnt_clear(clnt);
        free_views(&clnt->views, clnt->pool);
        dbr_prioq_term(&clnt->prioq);
        fig_cache_term(&clnt->cache);
        zmq_close(clnt->dealer);
        zmq_close(clnt->sub);
        free(clnt);
    }
}

DBR_API struct DbrSlNode*
dbr_clnt_find_rec_id(DbrClnt clnt, int type, DbrIden id)
{
    return fig_cache_find_rec_id(&clnt->cache, type, id);
}

DBR_API struct DbrSlNode*
dbr_clnt_find_rec_mnem(DbrClnt clnt, int type, const char* mnem)
{
    return fig_cache_find_rec_mnem(&clnt->cache, type, mnem);
}

DBR_API struct DbrSlNode*
dbr_clnt_first_rec(DbrClnt clnt, int type, size_t* size)
{
    return fig_cache_first_rec(&clnt->cache, type, size);
}

DBR_API DbrBool
dbr_clnt_empty_rec(DbrClnt clnt, int type)
{
    return fig_cache_empty_rec(&clnt->cache, type);
}

DBR_API DbrTrader
dbr_clnt_trader(DbrClnt clnt)
{
    return clnt->trader;
}

DBR_API DbrAccnt
dbr_clnt_accnt(DbrClnt clnt, struct DbrRec* arec)
{
    return fig_accnt_lazy(arec, clnt->pool);
}

DBR_API DbrIden
dbr_clnt_place(DbrClnt clnt, const char* accnt, const char* contr, DbrDate settl_date,
               const char* ref, int action, DbrTicks ticks, DbrLots lots, DbrLots min_lots)
{
    struct DbrBody body;
    body.req_id = clnt->id++;
    body.type = DBR_PLACE_ORDER_REQ;
    strncpy(body.place_order_req.accnt, accnt, DBR_MNEM_MAX);
    strncpy(body.place_order_req.contr, contr, DBR_MNEM_MAX);
    body.place_order_req.settl_date = settl_date;
    if (ref)
        strncpy(body.place_order_req.ref, ref, DBR_REF_MAX);
    else
        body.place_order_req.ref[0] = '\0';
    body.place_order_req.action = action;
    body.place_order_req.ticks = ticks;
    body.place_order_req.lots = lots;
    body.place_order_req.min_lots = min_lots;

    if (!dbr_prioq_push(&clnt->prioq, dbr_millis() + TIMEOUT, body.req_id))
        goto fail1;

    if (!dbr_send_body(clnt->dealer, &body, false))
        goto fail2;

    return body.req_id;
 fail2:
    dbr_prioq_clear(&clnt->prioq, body.req_id);
 fail1:
    return -1;
}

DBR_API DbrIden
dbr_clnt_revise_id(DbrClnt clnt, DbrIden id, DbrLots lots)
{
    struct DbrBody body;
    body.req_id = clnt->id++;
    body.type = DBR_REVISE_ORDER_ID_REQ;
    body.revise_order_id_req.id = id;
    body.revise_order_id_req.lots = lots;

    if (!dbr_prioq_push(&clnt->prioq, dbr_millis() + TIMEOUT, body.req_id))
        goto fail1;

    if (!dbr_send_body(clnt->dealer, &body, false))
        goto fail2;

    return body.req_id;
 fail2:
    dbr_prioq_clear(&clnt->prioq, body.req_id);
 fail1:
    return -1;
}

DBR_API DbrIden
dbr_clnt_revise_ref(DbrClnt clnt, const char* ref, DbrLots lots)
{
    struct DbrBody body;
    body.req_id = clnt->id++;
    body.type = DBR_REVISE_ORDER_REF_REQ;
    strncpy(body.revise_order_ref_req.ref, ref, DBR_REF_MAX);
    body.revise_order_ref_req.lots = lots;

    if (!dbr_prioq_push(&clnt->prioq, dbr_millis() + TIMEOUT, body.req_id))
        goto fail1;

    if (!dbr_send_body(clnt->dealer, &body, false))
        goto fail2;

    return body.req_id;
 fail2:
    dbr_prioq_clear(&clnt->prioq, body.req_id);
 fail1:
    return -1;
}

DBR_API DbrIden
dbr_clnt_cancel_id(DbrClnt clnt, DbrIden id)
{
    struct DbrBody body;
    body.req_id = clnt->id++;
    body.type = DBR_CANCEL_ORDER_ID_REQ;
    body.cancel_order_id_req.id = id;

    if (!dbr_prioq_push(&clnt->prioq, dbr_millis() + TIMEOUT, body.req_id))
        goto fail1;

    if (!dbr_send_body(clnt->dealer, &body, false))
        goto fail2;

    return body.req_id;
 fail2:
    dbr_prioq_clear(&clnt->prioq, body.req_id);
 fail1:
    return -1;
}

DBR_API DbrIden
dbr_clnt_cancel_ref(DbrClnt clnt, const char* ref)
{
    struct DbrBody body;
    body.req_id = clnt->id++;
    body.type = DBR_CANCEL_ORDER_REF_REQ;
    strncpy(body.cancel_order_ref_req.ref, ref, DBR_REF_MAX);

    if (!dbr_prioq_push(&clnt->prioq, dbr_millis() + TIMEOUT, body.req_id))
        goto fail1;

    if (!dbr_send_body(clnt->dealer, &body, false))
        goto fail2;

    return body.req_id;
 fail2:
    dbr_prioq_clear(&clnt->prioq, body.req_id);
 fail1:
    return -1;
}

DBR_API DbrIden
dbr_clnt_ack_trade(DbrClnt clnt, DbrIden id)
{
    struct DbrBody body;
    body.req_id = clnt->id++;
    body.type = DBR_ACK_TRADE_REQ;
    body.ack_trade_req.id = id;

    if (!dbr_prioq_push(&clnt->prioq, dbr_millis() + TIMEOUT, body.req_id))
        goto fail1;

    if (!dbr_send_body(clnt->dealer, &body, false))
        goto fail2;

    struct DbrExec* exec = fig_trader_release_trade_id(clnt->trader, id);
    // FIXME: reference-counted release.
    if (exec)
        dbr_pool_free_exec(clnt->pool, exec);

    return body.req_id;
 fail2:
    dbr_prioq_clear(&clnt->prioq, body.req_id);
 fail1:
    return -1;
}

DBR_API DbrBool
dbr_clnt_ready(DbrClnt clnt)
{
    return clnt->pending == 0;
}

DBR_API int
dbr_clnt_poll(DbrClnt clnt, int fd, int events, DbrMillis ms, struct DbrStatus* status)
{
    zmq_pollitem_t items[] = {
        { NULL,         fd, events,     0 },
        { clnt->sub,    0,  ZMQ_POLLIN, 0 },
        { clnt->dealer, 0,  ZMQ_POLLIN, 0 }
    };
    enum { LOCAL_FD, SUB_SOCK, DEALER_SOCK };

    // TODO: min of ms and timer.

    const int nevents = zmq_poll(items, 3, ms);
    if (nevents < 0) {
        dbr_err_setf(DBR_EIO, "zmq_poll() failed: %s", zmq_strerror(zmq_errno()));
        goto fail1;
    }

    status->req_id = 0;
    status->revents = items[LOCAL_FD].revents;
    status->sub = status->dealer = status->num = 0;
    status->msg[0] = '\0';

    if ((items[SUB_SOCK].revents & ZMQ_POLLIN)) {

        struct DbrBody body;
        if (!dbr_recv_body(clnt->sub, clnt->pool, &body))
            goto fail1;

        switch ((status->sub = body.type)) {
        case DBR_VIEW_LIST_REP:
            for (struct DbrSlNode* node = body.view_list_rep.first; node; ) {
                struct DbrView* view = dbr_shared_view_entry(node);
                node = node->next;
                // Transfer ownership.
                apply_viewup(clnt, view);
            }
            break;
        default:
            dbr_err_setf(DBR_EIO, "unknown body-type '%d'", body.type);
            goto fail1;
        }
    }
    if ((items[DEALER_SOCK].revents & ZMQ_POLLIN)) {

        struct DbrBody body;
        if (!dbr_recv_body(clnt->dealer, clnt->pool, &body))
            goto fail1;

        if ((status->req_id = body.req_id) > 0)
            dbr_prioq_clear(&clnt->prioq, body.req_id);

        switch ((status->dealer = body.type)) {
        case DBR_STATUS_REP:
            status->num = body.status_rep.num;
            strncpy(status->msg, body.status_rep.msg, DBR_ERRMSG_MAX);
            break;
        case DBR_TRADER_LIST_REP:
            emplace_rec_list(clnt, DBR_TRADER, body.entity_list_rep.first,
                             body.entity_list_rep.count_);
            break;
        case DBR_ACCNT_LIST_REP:
            emplace_rec_list(clnt, DBR_ACCNT, body.entity_list_rep.first,
                             body.entity_list_rep.count_);
            break;
        case DBR_CONTR_LIST_REP:
            emplace_rec_list(clnt, DBR_CONTR, body.entity_list_rep.first,
                             body.entity_list_rep.count_);
            break;
        case DBR_ORDER_LIST_REP:
            emplace_order_list(clnt, body.entity_list_rep.first);
            break;
        case DBR_EXEC_LIST_REP:
            emplace_exec_list(clnt, body.entity_list_rep.first);
            break;
        case DBR_MEMB_LIST_REP:
            emplace_memb_list(clnt, body.entity_list_rep.first);
            break;
        case DBR_POSN_LIST_REP:
            emplace_posn_list(clnt, body.entity_list_rep.first);
            break;
        case DBR_VIEW_LIST_REP:
            emplace_view_list(clnt, body.view_list_rep.first);
            break;
        case DBR_EXEC_REP:
            switch (body.exec_rep.exec->c.state) {
            case DBR_NEW:
                apply_new(clnt, body.exec_rep.exec);
                break;
            case DBR_REVISE:
            case DBR_CANCEL:
            case DBR_TRADE:
                apply_update(clnt, body.exec_rep.exec);
                break;
            }
            break;
        case DBR_POSN_REP:
            apply_posnup(clnt, body.posn_rep.posn);
            break;
        default:
            dbr_err_setf(DBR_EIO, "unknown body-type '%d'", body.type);
            goto fail1;
        }
    }
    return nevents;
 fail1:
    return -1;
}

DBR_API void
dbr_clnt_clear(DbrClnt clnt)
{
    struct DbrSlNode* node = clnt->execs.first;
    while (node) {
        struct DbrExec* exec = dbr_clnt_exec_entry(node);
        node = node->next;
        // Free completed orders.
        if (dbr_exec_done(exec)) {
            DbrTrader trader = exec->c.trader.rec->trader.state;
            assert(trader);
            struct DbrOrder* order = fig_trader_release_order_id(trader, exec->order);
            if (order)
                dbr_pool_free_order(clnt->pool, order);
        }
        // Trades are owned by trader.
        if (exec->c.state != DBR_TRADE)
            dbr_pool_free_exec(clnt->pool, exec);
    }
    dbr_queue_init(&clnt->execs);
    dbr_tree_init(&clnt->posnups);
    dbr_tree_init(&clnt->viewups);
}

DBR_API struct DbrSlNode*
dbr_clnt_first_exec(DbrClnt clnt)
{
    return dbr_queue_first(&clnt->execs);
}

DBR_API DbrBool
dbr_clnt_empty_exec(DbrClnt clnt)
{
    return dbr_queue_empty(&clnt->execs);
}

DBR_API struct DbrRbNode*
dbr_clnt_first_posnup(DbrClnt clnt)
{
    return dbr_tree_first(&clnt->posnups);
}

DBR_API struct DbrRbNode*
dbr_clnt_last_posnup(DbrClnt clnt)
{
    return dbr_tree_last(&clnt->posnups);
}

DBR_API DbrBool
dbr_clnt_empty_posnup(DbrClnt clnt)
{
    return dbr_tree_empty(&clnt->posnups);
}

DBR_API struct DbrRbNode*
dbr_clnt_find_view(DbrClnt clnt, DbrIden cid, DbrDate settl_date)
{
    const DbrIden key = dbr_book_key(cid, settl_date);
	return dbr_tree_find(&clnt->views, key);
}

DBR_API struct DbrRbNode*
dbr_clnt_first_view(DbrClnt clnt)
{
    return dbr_tree_first(&clnt->views);
}

DBR_API struct DbrRbNode*
dbr_clnt_last_view(DbrClnt clnt)
{
    return dbr_tree_last(&clnt->views);
}

DBR_API DbrBool
dbr_clnt_empty_view(DbrClnt clnt)
{
    return dbr_tree_empty(&clnt->views);
}

DBR_API struct DbrRbNode*
dbr_clnt_first_viewup(DbrClnt clnt)
{
    return dbr_tree_first(&clnt->viewups);
}

DBR_API struct DbrRbNode*
dbr_clnt_last_viewup(DbrClnt clnt)
{
    return dbr_tree_last(&clnt->viewups);
}

DBR_API DbrBool
dbr_clnt_empty_viewup(DbrClnt clnt)
{
    return dbr_tree_empty(&clnt->viewups);
}
