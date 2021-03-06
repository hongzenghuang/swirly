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
#ifndef SWIRLYUI_TRADEMODEL_HXX
#define SWIRLYUI_TRADEMODEL_HXX

#include "Exec.hxx"
#include "TableModel.hxx"

namespace swirly {
namespace ui {

class TradeModel
: public TableModel<ExecKey, Exec, unbox(exec::Column::CheckState), exec::ColumnCount> {
  public:
    TradeModel(QObject* parent = nullptr);
    ~TradeModel() override;

    QVariant data(const QModelIndex& index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void remove_row(std::uint64_t tag, const Exec& trade)
    {
        const ExecKey key{trade.market_id(), trade.id()};
        TableModel::remove_row(key);
    }
    void update_row(std::uint64_t tag, const Exec& trade)
    {
        const ExecKey key{trade.market_id(), trade.id()};
        TableModel::update_row(key, tag, trade);
    }

  private:
    QVariant header_[exec::ColumnCount];
};

} // namespace ui
} // namespace swirly

#endif // SWIRLYUI_TRADEMODEL_HXX
