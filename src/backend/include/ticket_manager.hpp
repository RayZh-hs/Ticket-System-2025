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

    struct TrainFareSegment {
        using station_id_t = hash_t;
        using price_t = int;

        price_t price = 0; // the price from this station to the next
        int remaining_seats = 0;

        TrainFareSegment operator&(const TrainFareSegment &other) const {
            return {price + other.price, std::min(remaining_seats, other.remaining_seats)};
        }
    };

    struct TrainFare {
        using SegmentList = norb::FiledSegmentList<TrainFareSegment>;

        train_id_t train_id;
        SegmentList::SegmentPointer segment_pointer;

        // both sides are inclusive
        TrainFareSegment join_segments(const SegmentList &seg_ref, const int from, const int to) const {
            if (from < 0 || to >= segment_pointer.size || from > to) {
                throw std::runtime_error("Invalid segment range query");
            }
            auto ans = seg_ref.get(segment_pointer, from);
            for (int i = from + 1; i <= to; ++i) {
                ans = ans & seg_ref.get(segment_pointer, i);
            }
            return ans;
        }

        bool operator!=(const TrainFare &other) const {
            return train_id != other.train_id;
        }

        auto operator<=>(const TrainFare &other) const {
            return train_id <=> other.train_id;
        }
    };

    struct Order {
        using account_id_t = hash_t;
        using train_group_id_t = hash_t;
        using Date = norb::Datetime::Date;
        using Datetime = norb::Datetime;
        using timestamp_t = int;
        using price_t = int;
        enum Status { Success, Pending, Refunded };

        account_id_t account;
        train_id_t train_id;
        int from_station_serial = 0;
        int to_station_serial = 0;
        Datetime from_time;             // the datetime of departure
        Datetime to_time;               // the datetime of arrival
        timestamp_t purchase_timestamp; // the datetime of first purchase
        int count = 0;
        price_t total_price;
        Status status = Status::Success;

        using order_id_t = norb::Pair<account_id_t, timestamp_t>;
        order_id_t id() const {
            return {account, purchase_timestamp};
        }
        static order_id_t id_from_account_and_timestamp(const account_id_t &account, const timestamp_t &timestamp) {
            return {account, timestamp};
        }

        std::string status_string() const {
            switch (status) {
            case Success:
                return "success";
            case Pending:
                return "pending";
            case Refunded:
                return "refunded";
            default:
                return "UNKNOWN";
            }
        }
    };

    class TicketManager {
      public:
        using price_t = int;
        using station_id_t = global_hash_method::hash_t;
        using timestamp_t = Order::timestamp_t;
        using order_id_t = Order::order_id_t;
        using interface = global_interface;

      private:
        using LogLevel = norb::LogLevel;
        using TrainStatusSegmentPointer = TrainFare::SegmentList::SegmentPointer;

        norb::BPlusTree<Order::order_id_t, Order, norb::MANUAL> purchase_history_store;
        norb::BPlusTree<norb::Pair<train_id_t, timestamp_t>, order_id_t, norb::MANUAL> pending_order_store;
        ;
        norb::BPlusTree<train_id_t, TrainFare, norb::MANUAL> train_fare_store;
        norb::FiledSegmentList<TrainFareSegment> train_fare_segments;

        struct TemporalTrainGroupInfo {
            norb::vector<price_t> prices;
            norb::Range<Date> sale_date_range;
            int seat_num = 0;
        };
        norb::BPlusTree<train_group_id_t, TemporalTrainGroupInfo, norb::MANUAL> temporary_train_group_info_store;

      public:
        TicketManager() : train_fare_segments(train_fare_segments_name) {
        }

        // this will not check the validity of the train group ID
        void add_train_group(const train_group_id_t &train_group_id, const norb::vector<price_t> &prices,
                             const norb::Range<Date> &sale_date_range, const int &seat_num) {
            assert(temporary_train_group_info_store.count(train_group_id) == 0);
            temporary_train_group_info_store.insert(train_group_id, {prices, sale_date_range, seat_num});
        }

        // this will not check the validity of the train group ID, or that it has not been released
        void release_train_group(const train_group_id_t &train_group_id) {
            const auto [prices, sale_date_range, seat_num] =
                temporary_train_group_info_store.find_first(train_group_id).value();
            for (Date date = sale_date_range.get_from(); date <= sale_date_range.get_to(); ++date) {
                auto segment_pointer = train_fare_segments.allocate(prices.size());
                interface::log.as(LogLevel::DEBUG) << "Allocated segment pointer: (cur=" << segment_pointer.cur
                                                   << ", size=" << segment_pointer.size << ")\n";
                for (size_t i = 0; i < prices.size(); ++i) {
                    train_fare_segments.set(segment_pointer, i, {prices[i], seat_num});
                    interface::log.as(LogLevel::DEBUG)
                        << "Segment set to: price=" << prices[i] << " seats=" << seat_num << "\n";
                }
                const auto train_id = train_id_t{train_group_id, date};
                train_fare_store.insert(train_id, {train_id, segment_pointer});
            }
        }

        void remove_train_group(const train_group_id_t &train_group_id) {
            temporary_train_group_info_store.remove_all(train_group_id);
            interface::log.as(LogLevel::DEBUG)
                << "From ticket_manager: Removed train group with ID: " << train_group_id << "\n";
        }

        auto get_train_status(const train_id_t &train_id) const {
            return train_fare_store.find_first(train_id);
        }

        auto get_train_status_station_segment(const TrainStatusSegmentPointer &seg_ptr, const int cursor) const {
            return train_fare_segments.get(seg_ptr, cursor);
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
                remaining_seats.push_back(
                    get_train_status_station_segment(train_status->segment_pointer, i).remaining_seats);
            }
            return remaining_seats;
        }

        // receive using [price, remaining_seats] for segment dispatch
        auto get_price_seat_for_section(const train_id_t &train_id, const int from_serial, const int to_serial) const {
            const auto train_status = get_train_status(train_id);
            if (not train_status.has_value()) {
                throw std::runtime_error("Train status not found.");
            }
            if (from_serial < 0 || to_serial - 1 >= train_status->segment_pointer.size || from_serial > to_serial) {
                throw std::out_of_range("Invalid segment range query");
            }
            return train_status->join_segments(train_fare_segments, from_serial, to_serial - 1);
        }

        void register_order(const Order &order) {
            if (purchase_history_store.count(order.id())) {
                // this should not happen, unless timestamps are not unique
                throw std::runtime_error("Order already exists.");
            }
            purchase_history_store.insert(order.id(), order);
            if (order.Pending) {
                pending_order_store.insert({order.train_id, order.purchase_timestamp}, order.id());
            } else {
                // update the number of remaining seats
                auto train_status = get_train_status(order.train_id);
                if (not train_status.has_value()) {
                    throw std::runtime_error("Train status not found."); // this should not happen
                }
                auto segment_pointer = train_status->segment_pointer;
                for (int i = order.from_station_serial; i < order.to_station_serial; ++i) {
                    auto segment = train_fare_segments.get(segment_pointer, i);
                    assert(segment.remaining_seats >= order.count && "Not enough seats available");
                    segment.remaining_seats -= order.count;
                    train_fare_segments.set(segment_pointer, i, segment);
                }
            }
        }

        norb::vector<Order> get_orders_by_account(const Order::account_id_t &account_id) {
            norb::vector<Order> orders = purchase_history_store.find_all_in_range(
                norb::unpack_range(account_id, norb::Range<Order::timestamp_t>::full_range()));
            return orders;
        }

        void refund_order(Order order) {
            const auto order_id = order.id();
            purchase_history_store.remove(order_id, order);
            if (order.status == Order::Status::Refunded) {
                throw std::runtime_error("Order already refunded.");
            }
            order.status = Order::Status::Refunded;
            purchase_history_store.insert(order_id, order);
            // update the number of remaining seats
            const auto train_status = get_train_status(order.train_id);
            if (not train_status.has_value()) {
                throw std::runtime_error("Train status not found."); // this should not happen
            }
            const auto segment_pointer = train_status->segment_pointer;
            for (int i = order.from_station_serial; i < order.to_station_serial; ++i) {
                auto segment = train_fare_segments.get(segment_pointer, i);
                segment.remaining_seats += order.count;
                train_fare_segments.set(segment_pointer, i, segment);
            }
            // look for pending orders that can now be verified
            auto pending_orders = pending_order_store.find_all_in_range(
                norb::unpack_range(order.train_id, norb::Range<Order::timestamp_t>::full_range())
            );
            for (const auto &pending_order_id : pending_orders) {
                auto pending_order = purchase_history_store.find_first(pending_order_id).value();
                assert(pending_order.status == Order::Status::Pending &&
                       "Pending order should be in pending status");
                const auto [_, remaining_seats] = get_price_seat_for_section(
                    pending_order.train_id, pending_order.from_station_serial, pending_order.to_station_serial);
                if (remaining_seats >= pending_order.count) {
                    // this order can be satisfied
                    assert(purchase_history_store.remove(pending_order_id, pending_order));
                    assert(pending_order_store.remove(
                        {pending_order.train_id, pending_order.purchase_timestamp}, pending_order_id
                    ));
                    pending_order.status = Order::Status::Success;
                    purchase_history_store.insert(pending_order_id, pending_order);
                    // update the number of remaining seats
                    for (int i = pending_order.from_station_serial; i < pending_order.to_station_serial; ++i) {
                        auto segment = train_fare_segments.get(segment_pointer, i);
                        assert(segment.remaining_seats >= pending_order.count && "Not enough seats available");
                        segment.remaining_seats -= pending_order.count;
                        train_fare_segments.set(segment_pointer, i, segment);
                    }
                    interface::log.as(LogLevel::DEBUG)
                        << "Pending order " << pending_order_id << " has been successfully processed.\n";
                }
            }
        }

        void clear() {
            purchase_history_store.clear();
            pending_order_store.clear();
            train_fare_store.clear();
            temporary_train_group_info_store.clear();
            // train_fare_segments = norb::FiledSegmentList<TrainFareSegment>(train_fare_segments_name);
            interface::log.as(LogLevel::DEBUG) << "TicketManager cleared.\n";
        }
    };
} // namespace ticket
