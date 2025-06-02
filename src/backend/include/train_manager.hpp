#pragma once

#include <cassert>

#include "b_plus_tree.hpp"
#include "datetime.hpp"
#include "stlite/filed_list.hpp"
#include "stlite/fixed_string.hpp"
#include "stlite/pair.hpp"
#include "stlite/range.hpp"

#include "settings.hpp"

namespace ticket {
    template <typename T_> using SegmentList = norb::FiledSegmentList<T_>;
    using norb::Datetime;
    using norb::DeltaDatetime;
    using Date = Datetime::Date;
    using Time = Datetime::Time;
    using hash_t = global_hash_method::hash_t;

    struct TrainGroupStationSegment {
        using station_id_t = hash_t;
        using price_t = int;

        station_id_t station_id;
        DeltaDatetime arrival_time;
        DeltaDatetime departure_time;
        price_t price;
    };

    struct TrainGroup {
        using train_group_name_t = norb::FixedUTF8String<max_train_id_length>;
        using train_group_id_t = hash_t;
        using seat_num_t = int;
        using type_t = char;

        train_group_name_t train_group_name;
        SegmentList<TrainGroupStationSegment>::SegmentPointer segment_pointer = {0, 0};
        seat_num_t seat_num = 0;
        norb::Range<Date> sale_date_range;
        type_t train_type = '\0';

        [[nodiscard]] auto hash() const {
            return global_hash_method::hash(static_cast<std::string>(train_group_name));
        }

        bool operator!=(const TrainGroup &other) const {
            return hash() != other.hash();
        }

        auto operator<=>(const TrainGroup &other) const {
            return hash() <=> other.hash();
        }
    };

    class TrainManager {
      public:
        using train_group_id_t = TrainGroup::train_group_id_t;
        using station_name_t = norb::FixedUTF8String<max_station_name_characters * max_bytes_per_chinese_char>;
        using interface = global_interface;
        using LogLevel = norb::LogLevel;

      private:
        norb::BPlusTree<train_group_id_t, TrainGroup, norb::MANUAL> train_group_store;
        norb::BPlusTree<train_group_id_t, bool, norb::MANUAL> train_group_release_store;
        norb::BPlusTree<train_group_id_t, station_name_t, norb::MANUAL> train_group_station_name_store;
        SegmentList<TrainGroupStationSegment> train_group_segments;

      public:
        TrainManager() : train_group_segments(train_group_segments_name) {
        }

        static auto train_group_id_from_name(const std::string &name) {
            return global_hash_method::hash(name);
        }

        static auto station_id_from_name(const std::string &name) {
            return global_hash_method::hash(name);
        }

        void add_train_group(const std::string &train_group_name, const std::vector<TrainGroupStationSegment> &segments,
                             const int &seat_num, const Date &sale_start_date, const Date &sale_end_date,
                             const char &train_type) {
            assert(seat_num > 0);
            assert(sale_start_date <= sale_end_date);

            train_group_id_t train_group_id = train_group_id_from_name(train_group_name);
            if (train_group_store.count(train_group_id)) {
                throw std::runtime_error("Train group already exists.");
            }

            TrainGroup new_train_group = {.train_group_name = train_group_name,
                                          .seat_num = seat_num,
                                          .sale_date_range = norb::Range<Date>(sale_start_date, sale_end_date),
                                          .train_type = train_type};

            auto segment_pointer = train_group_segments.allocate(segments.size());
            interface::log.as(LogLevel::DEBUG) << "Allocated segment pointer: (cur=" << segment_pointer.cur
                                               << ", size=" << segment_pointer.size << ")\n";
            for (size_t i = 0; i < segments.size(); ++i) {
                train_group_segments.set(segment_pointer, i, segments[i]);
            }
            new_train_group.segment_pointer = segment_pointer;

            train_group_store.insert(train_group_id, new_train_group);
            train_group_release_store.insert(train_group_id, false);
        }

        void release_train_group(const train_group_id_t &train_group_id) {
            if (not train_group_store.count(train_group_id)) {
                throw std::runtime_error("Train group does not exist.");
            }
            if (train_group_release_store.find_first(train_group_id).value()) {
                throw std::runtime_error("Train group is already released.");
            }
            train_group_release_store.remove(train_group_id, false);
            train_group_release_store.insert(train_group_id, true);
        }

        void delete_train_group(const train_group_id_t &train_group_id) {
            if (not train_group_store.count(train_group_id)) {
                throw std::runtime_error("Train group does not exist.");
            }
            if (train_group_release_store.find_first(train_group_id).value()) {
                throw std::runtime_error("Train group is released and cannot be deleted.");
            }
            // remove in train_group_store and train_group_release_store
            // there is no need to remove segments from train_group_segments since it is a dynamic segment list
            assert(train_group_store.remove_all(train_group_id));
            assert(train_group_release_store.remove(train_group_id, false));
        }
    };
} // namespace ticket
