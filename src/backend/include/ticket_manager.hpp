#pragma once

#include "datetime.hpp"
#include "logging.hpp"
#include "stlite/filed_list.hpp"
#include "stlite/pair.hpp"

#include "settings.hpp"
#include "utility/wrappers.hpp"

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
        using station_id_t = global_hash_method::hash_t;
        using interface = global_interface;

      private:
        using LogLevel = norb::LogLevel;
        using TrainStatusSegmentPointer = TrainStatus::SegmentList::SegmentPointer;

        norb::BPlusTree<Order::order_id_t, Order, norb::MANUAL> purchase_history_store;
        norb::BPlusTree<train_id_t, TrainStatus, norb::MANUAL> train_status_store;
        norb::FiledSegmentList<TrainStatusStationSegment> train_status_station_segments;

        struct TemporalTrainGroupInfo {
            norb::vector<price_t> prices;
            norb::Range<Date> sale_date_range;
            int seat_num = 0;
        };
        norb::BPlusTree<train_group_id_t, TemporalTrainGroupInfo, norb::MANUAL> temporary_train_group_info_store;

      public:
        TicketManager() : train_status_station_segments(ticket_hub_segments_name) {
        }

        // this will not check the validity of the train group ID
        void add_train_group(const train_group_id_t &train_group_id, const norb::vector<price_t> &prices,
                             const norb::Range<Date> &sale_date_range, const int &seat_num) {
            assert(temporary_train_group_info_store.count(train_group_id) == 0);
            temporary_train_group_info_store.insert(train_group_id, {prices, sale_date_range, seat_num});
        }

        // this will not check the validity of the train group ID, or that it has not been released
        void release_train_group(const train_group_id_t &train_group_id) {
            const auto [
                prices, sale_date_range, seat_num
            ] = temporary_train_group_info_store.find_first(train_group_id).value();
            for (Date date = sale_date_range.get_from(); date <= sale_date_range.get_to(); ++date) {
                auto segment_pointer = train_status_station_segments.allocate(prices.size());
                interface::log.as(LogLevel::DEBUG) << "Allocated segment pointer: (cur=" << segment_pointer.cur
                                                   << ", size=" << segment_pointer.size << ")\n";
                for (size_t i = 0; i < prices.size(); ++i) {
                    train_status_station_segments.set(segment_pointer, i, {prices[i], seat_num});
                    interface::log.as(LogLevel::DEBUG)
                        << "Segment set to: price=" << prices[i] << " seats=" << seat_num << "\n";
                }
                const auto train_id = train_id_t{train_group_id, date};
                train_status_store.insert(train_id, {train_id, segment_pointer});
            }
        }

        void remove_train_group(const train_group_id_t &train_group_id) {
            temporary_train_group_info_store.remove_all(train_group_id);
            interface::log.as(LogLevel::DEBUG)
                << "From ticket_manager: Removed train group with ID: " << train_group_id << "\n";
        }

        auto get_train_status(const train_id_t &train_id) const {
            return train_status_store.find_first(train_id);
        }

        auto get_train_status_station_segment(const TrainStatusSegmentPointer &seg_ptr, const int cursor) const {
            return train_status_station_segments.get(seg_ptr, cursor);
        }

        auto get_remaining_seats(const train_id_t &train_id, const int station_serial) const {
            const auto train_status = get_train_status(train_id);
            if (not train_status.has_value()) {
                throw std::runtime_error("Train status not found.");
            }
            if (station_serial < 0 || station_serial >= train_status->segment_pointer.size) {
                throw std::out_of_range("Station serial out of range.");
            }
            return get_train_status_station_segment(train_status->segment_pointer, station_serial).remaining_seats;
        }

        auto get_remaining_seats_for_train(const train_id_t &train_id) const {
            const auto train_status = get_train_status(train_id);
            if (not train_status.has_value()) {
                throw std::runtime_error("Train status not found.");
            }
            norb::vector<int> remaining_seats;
            for (int i = 0; i < train_status->segment_pointer.size; ++i) {
                remaining_seats.push_back(get_train_status_station_segment(train_status->segment_pointer, i).remaining_seats);
            }
            return remaining_seats;
        }

        // receive using [price, remaining_seats] for segment dispatch
        auto get_price_seat_for_section(const train_id_t &train_id, const int from_serial, const int to_serial) const {
            const auto train_status = get_train_status(train_id);
            if (not train_status.has_value()) {
                throw std::runtime_error("Train status not found.");
            }
            if (from_serial < 0 || to_serial >= train_status->segment_pointer.size || from_serial > to_serial) {
                throw std::out_of_range("Invalid segment range query");
            }
            return train_status->join_segments(train_status_station_segments, from_serial, to_serial);
        }

        // struct QueryReturnStructure {
        //     train_id_t train_id;
        //     Datetime from_time;
        //     station_id_t from_station;
        //     Datetime to_time;
        //     station_id_t to_station;
        // };

        // // find all trains that leave at from at date and arrive in to
        // auto query_ticket(const station_id_t &from_station, const station_id_t &to_station, const Date &date) const {
        //     norb::vector<QueryReturnStructure> results;
        //     for (const auto &[train_id, train_status] : train_status_store) {
        //         if (train_id.second != date) {
        //             continue; // only consider the train on the specific date
        //         }
        //         const auto &seg_ptr = train_status.segment_pointer;
        //         if (seg_ptr.size < 2) {
        //             continue; // not enough segments to query
        //         }
        //         const auto from_segment = get_train_status_station_segment(seg_ptr, 0);
        //         const auto to_segment = get_train_status_station_segment(seg_ptr, seg_ptr.size - 1);
        //         if (from_segment.remaining_seats > 0 && from_segment.price > 0 &&
        //             from_segment.price < std::numeric_limits<price_t>::max() &&
        //             from_segment.remaining_seats < std::numeric_limits<int>::max()) {
        //             results.push_back({train_id,
        //                                {date, from_segment.departure_time, from_station},
        //                                {date, to_segment.arrival_time, to_station}});
        //         }
        //     }
        //     return results;
        // }
    };
} // namespace ticket
