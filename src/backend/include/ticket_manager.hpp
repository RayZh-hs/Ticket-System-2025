#pragma once

#include "datetime.hpp"
#include "logging.hpp"
#include "stlite/filed_list.hpp"
#include "stlite/pair.hpp"


#include "settings.hpp"

#include <utility>

namespace ticket {
    using hash_t = global_hash_method::hash_t;
    using train_group_id_t = hash_t;
    using Date = norb::Datetime::Date;

    using train_id_t = norb::Pair<train_group_id_t, Date>;

    struct TrainStatusStationSegment {
        using station_id_t = hash_t;
        using price_t = int;

        price_t price; // the price from this station to the next
        int remaining_seats = 0;

        TrainStatusStationSegment operator&(const TrainStatusStationSegment &other) const {
            return {price + other.price, std::min(remaining_seats, other.remaining_seats)};
        }
    };

    struct TrainStatus {
        using SegmentList = norb::FiledSegmentList<TrainStatusStationSegment>;

        train_id_t train_id;
        SegmentList::SegmentPointer segment_pointer;

        TrainStatusStationSegment join_segments(const SegmentList &seg_ref, const int from, const int to) const {
            if (from < 0 || to >= segment_pointer.size || from > to) {
                throw std::runtime_error("Invalid segment range query");
            }
            auto ans = seg_ref.get(segment_pointer, from);
            for (int i = from + 1; i <= to; ++i) {
                ans = ans & seg_ref.get(segment_pointer, i);
            }
            return ans;
        }

        bool operator!=(const TrainStatus &other) const {
            return train_id != other.train_id;
        }

        auto operator<=>(const TrainStatus &other) const {
            return train_id <=> other.train_id;
        }
    };

    struct Order {
        using account_id_t = hash_t;
        using train_group_id_t = hash_t;
        using Date = norb::Datetime::Date;
        using timestamp_t = int;
        using price_t = int;

        account_id_t account;
        train_id_t train_id;
        timestamp_t purchase_timestamp;
        price_t total_price;
        bool is_pending = false;

        using order_id_t = norb::Pair<account_id_t, timestamp_t>;
        order_id_t id() const {
            return {account, purchase_timestamp};
        }
        static order_id_t id_from_account_and_timestamp(const account_id_t &account, const timestamp_t &timestamp) {
            return {account, timestamp};
        }
    };

    class TicketManager {
      public:
        using price_t = int;
        using interface = global_interface;

      private:
        using LogLevel = norb::LogLevel;

        norb::BPlusTree<Order::order_id_t, Order, norb::MANUAL> purchase_history_store;
        norb::BPlusTree<train_id_t, TrainStatus, norb::MANUAL> train_status_store;
        norb::FiledSegmentList<TrainStatusStationSegment> ticket_hub_segments;

      public:
        TicketManager() : ticket_hub_segments(ticket_hub_segments_name) {
        }

        void add_train_group(const train_group_id_t &train_group_id, const norb::vector<price_t> &prices,
                             const norb::Range<Date> &sale_date_range, const int &seat_num) {
            if (train_status_store.count({train_group_id, sale_date_range.get_from()})) {
                throw std::runtime_error("Train group already exists.");
            }
            const auto one_day = norb::Datetime::Date(0, 1);
            for (Date date = sale_date_range.get_from(); date <= sale_date_range.get_to(); date = date + one_day) {
                auto segment_pointer = ticket_hub_segments.allocate(prices.size());
                interface::log.as(LogLevel::DEBUG) << "Allocated segment pointer: (cur=" << segment_pointer.cur
                                                   << ", size=" << segment_pointer.size << ")\n";
                for (size_t i = 0; i < prices.size(); ++i) {
                    ticket_hub_segments.set(segment_pointer, i, {prices[i], seat_num});
                    interface::log.as(LogLevel::DEBUG) << "Segment set to: price=" << prices[i] << " seats=" << seat_num << "\n";
                }
                const auto train_id = train_id_t{train_group_id, date};
                train_status_store.insert(train_id, {train_id, segment_pointer});
            }
        }
    };
} // namespace ticket
