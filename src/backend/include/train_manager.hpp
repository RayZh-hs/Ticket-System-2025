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

        station_id_t station_id = 0;
        DeltaDatetime arrival_time;
        DeltaDatetime departure_time;
        price_t price = 0;
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
        using station_id_t = hash_t;
        using interface = global_interface;
        using LogLevel = norb::LogLevel;

        struct StationLookupStruct {
            train_group_id_t train_group_id = 0;
            int station_serial = 0;
            Datetime delta_arrival_time, delta_departure_time;

            using id_t = train_group_id_t;
            [[nodiscard]] id_t id() const {
                return train_group_id;
            }
        };

      private:
        norb::BPlusTree<train_group_id_t, TrainGroup, norb::MANUAL> train_group_store;
        norb::BPlusTree<train_group_id_t, bool, norb::MANUAL> train_group_release_store;
        norb::BPlusTree<station_id_t, station_name_t, norb::MANUAL> station_name_store;
        // this lookup table keeps track of all RELEASED stores
        norb::BPlusTree<station_id_t, StationLookupStruct, norb::AUTOMATIC> station_train_group_lookup_store;
        SegmentList<TrainGroupStationSegment> train_group_segments;
        using TrainGroupSegmentPointer = SegmentList<TrainGroupStationSegment>::SegmentPointer;

      public:
        TrainManager() : train_group_segments(train_group_segments_name) {
        }

        static auto train_group_id_from_name(const std::string &name) {
            return global_hash_method::hash(name);
        }

        static auto station_id_from_name(const std::string &name) {
            return global_hash_method::hash(name);
        }

        std::optional<station_name_t> station_name_from_id(const station_id_t &id) const {
            return station_name_store.find_first(id);
        }
        
        bool exists_train_group(const station_id_t &station_id) const {
            return train_group_store.count(station_id);
        }

        bool has_released_train_group(const train_group_id_t &train_group_id) const {
            return train_group_release_store.find_first(train_group_id).value_or(false);
        }

        void register_station(const station_name_t &station_name) {
            const auto station_id = station_id_from_name(static_cast<std::string>(station_name));
            if (not station_name_store.count(station_id)) {
                station_name_store.insert(station_id, station_name);
            }
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

            // append the group info into the lookup table
            const auto train_group_info = train_group_store.find_first(train_group_id);
            assert(train_group_info.has_value() && "Train group should exist when releasing it");
            const auto &segment_pointer = train_group_info->segment_pointer;
            // for each segment, register the station and its train group
            for (int i = 0; i < segment_pointer.size; ++i) {
                const auto &segment = train_group_segments.get(segment_pointer, i);
                const auto station_id = segment.station_id;
                // insert into the lookup table
                station_train_group_lookup_store.insert(
                    station_id, StationLookupStruct{train_group_id, i, segment.arrival_time, segment.departure_time});
            }
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

        std::optional<TrainGroup> get_train_group(const train_group_id_t &train_group_id) const {
            return train_group_store.find_first(train_group_id);
        }

        auto get_train_group_segment(const TrainGroupSegmentPointer &seg_ptr, const int cursor) const {
            return train_group_segments.get(seg_ptr, cursor);
        }

        auto get_trains_from_station(const station_id_t &station_id) const {
            return station_train_group_lookup_store.find_all(station_id);
        }

        // this function does not check for validity
        int search_for_dest(const train_group_id_t &train_group_id, const station_id_t &bound_station_id) const {
            const auto seg_ptr = train_group_store.find_first(train_group_id).value().segment_pointer;
            for (int i = seg_ptr.size - 1; i >= 0; --i) {
                const auto &segment = train_group_segments.get(seg_ptr, i);
                if (segment.station_id == bound_station_id) {
                    return i;
                }
            }
            return -1; // not found
        }

        bool is_train_bound_for(const train_group_id_t &train_group_id, const station_id_t &bound_station_id,
                                const int after_serial) const {
            const auto dest_search = search_for_dest(train_group_id, bound_station_id);
            if (dest_search < 0) {
                return false; // not found
            } else {
                return dest_search > after_serial; // found and after the serial
            }
        }

        auto get_arrival_date_range(const train_group_id_t &train_group_id, const int station_serial) const {
            const auto train_group_info = train_group_store.find_first(train_group_id).value();
            const auto seg_ptr = train_group_info.segment_pointer;
            if (station_serial < 0 || station_serial >= seg_ptr.size) {
                throw std::out_of_range("Station serial out of range.");
            }
            const auto &segment = train_group_segments.get(seg_ptr, station_serial);
            return norb::Range(train_group_info.sale_date_range.get_from() + segment.arrival_time.getDate(),
                               train_group_info.sale_date_range.get_to() + segment.arrival_time.getDate());
        }
    };
} // namespace ticket
