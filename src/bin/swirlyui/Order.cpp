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
#include "Order.hpp"

#include "Json.hpp"

#include <QDebug>
#include <QJsonObject>

namespace swirly {
namespace ui {

Order Order::fromJson(const QJsonObject& obj)
{
  using swirly::ui::fromJson;
  return Order{fromJson<QString>(obj["accnt"]),     fromJson<QString>(obj["contr"]),
               fromJson<QDate>(obj["settlDate"]),   fromJson<Id64>(obj["id"]),
               fromJson<QString>(obj["ref"]),       fromJson<State>(obj["state"]),
               fromJson<Side>(obj["side"]),         fromJson<Lots>(obj["lots"]),
               fromJson<Ticks>(obj["ticks"]),       fromJson<Lots>(obj["resd"]),
               fromJson<Lots>(obj["exec"]),         fromJson<Cost>(obj["cost"]),
               fromJson<Lots>(obj["lastLots"]),     fromJson<Ticks>(obj["lastTicks"]),
               fromJson<Lots>(obj["minLots"]),      fromJson<QDateTime>(obj["created"]),
               fromJson<QDateTime>(obj["modified"])};
}

QDebug operator<<(QDebug debug, const Order& order)
{
  debug.nospace() << "accnt=" << order.accnt() //
                  << ",contr=" << order.contr() //
                  << ",settlDate=" << order.settlDate() //
                  << ",id=" << order.id() //
                  << ",ref=" << order.ref() //
                  << ",state=" << order.state() //
                  << ",side=" << order.side() //
                  << ",lots=" << order.lots() //
                  << ",ticks=" << order.ticks() //
                  << ",resd=" << order.resd() //
                  << ",exec=" << order.exec() //
                  << ",cost=" << order.cost() //
                  << ",lastLots=" << order.lastLots() //
                  << ",lastTicks=" << order.lastTicks() //
                  << ",minLots=" << order.minLots() //
                  << ",created=" << order.created() //
                  << ",modified=" << order.modified();
  return debug;
}

} // ui
} // swirly