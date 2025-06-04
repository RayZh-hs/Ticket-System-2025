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
    using norb::Datetime;
    using norb::DeltaDatetime;
    using Date = Datetime::Date;
    using Time = Datetime::Time;
    using hash_t = global_hash_method::hash_t;

    struct TrainGroupSegment {
        using station_id_t = hash_t;
        using price_t = int;

        station_id_t station_id = 0;
        DeltaDatetime arrival_time;
        DeltaDatetime departure_time;
        price_t price = 0;
    };

    struct TrainGroup {
        using SegmentList = norb::FiledSegmentList<TrainGroupSegment>;
        using train_group_name_t = norb::FixedUTF8String<max_train_id_length>;
        using train_group_id_t = hash_t;
        using seat_num_t = int;
        using type_t = char;

        train_group_name_t train_group_name;
        SegmentList::SegmentPointer segment_pointer = {0, 0};
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
            int station_from_serial = 0;
            int station_to_serial = 0;

            using id_t = train_group_id_t;
            [[nodiscard]] id_t id() const {
                return train_group_id;
            }
        };

      private:
        using SegmentList = norb::FiledSegmentList<TrainGroupSegment>;
        using TrainGroupSegmentPointer = SegmentList::SegmentPointer;
        norb::BPlusTree<train_group_id_t, TrainGroup, norb::MANUAL> train_group_store;
        norb::BPlusTree<train_group_id_t, bool, norb::MANUAL> train_group_release_store;
        norb::BPlusTree<station_id_t, station_name_t, norb::MANUAL> station_name_store;
        // this lookup table keeps track of all RELEASED stores
        // format:
        norb::BPlusTree<norb::Pair<station_id_t, station_id_t>, StationLookupStruct, norb::AUTOMATIC>
            station_train_group_lookup_store;
        SegmentList train_group_segments;

      public:
        norb::vector<station_id_t> station_id_vector;
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
                station_id_vector.push_back(station_id);
            }
        }

        void add_train_group(const std::string &train_group_name, const std::vector<TrainGroupSegment> &segments,
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
                const auto from_station_id = train_group_segments.get(segment_pointer, i).station_id;
                for (int j = i + 1; j < segment_pointer.size; ++j) {
                    const auto to_station_id = train_group_segments.get(segment_pointer, j).station_id;
                    // insert into the lookup table
                    station_train_group_lookup_store.insert({from_station_id, to_station_id},
                                                            StationLookupStruct{train_group_id, i, j});
                }
            }
            interface::log.as(LogLevel::DEBUG) << "The lookup table in TrainManager has been updated.\n";
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

        auto get_departure_date_range(const train_group_id_t &train_group_id, const int station_serial) const {
            const auto train_group_info = train_group_store.find_first(train_group_id).value();
            const auto seg_ptr = train_group_info.segment_pointer;
            if (station_serial < 0 || station_serial >= seg_ptr.size) {
                throw std::out_of_range("Station serial out of range.");
            }
            const auto &segment = train_group_segments.get(seg_ptr, station_serial);
            return norb::Range(train_group_info.sale_date_range.get_from() + segment.departure_time.to_days(),
                               train_group_info.sale_date_range.get_to() + segment.departure_time.to_days());
        }

        auto get_departure_datetime_range(const train_group_id_t &train_group_id, const int station_serial) const {
            const auto train_group_info = train_group_store.find_first(train_group_id).value();
            const auto seg_ptr = train_group_info.segment_pointer;
            if (station_serial < 0 || station_serial >= seg_ptr.size) {
                throw std::out_of_range("Station serial out of range.");
            }
            const auto &segment = train_group_segments.get(seg_ptr, station_serial);
            return norb::Range(
                Datetime(train_group_info.sale_date_range.get_from()) + segment.departure_time.to_minutes(),
                Datetime(train_group_info.sale_date_range.get_to()) + segment.departure_time.to_minutes());
        }

        Date deduce_departure_date_from(const train_group_id_t &train_group_id, const int &station_serial,
                                        const Date &date_at_station) const {
            const auto &train_group_info = train_group_store.find_first(train_group_id).value();
            const TrainGroupSegment &from_station_segment =
                train_group_segments.get(train_group_info.segment_pointer, station_serial);
            return date_at_station - from_station_segment.arrival_time.to_days();
        }

        Date deduce_departure_date_from(const TrainGroup &group_train_info, const int &station_serial,
                                        const Date &date_at_station) const {
            const TrainGroupSegment &from_station_segment =
                train_group_segments.get(group_train_info.segment_pointer, station_serial);
            return date_at_station - from_station_segment.arrival_time.to_days();
        }

        Date deduce_departure_date_from(const TrainGroup &group_train_info, const int &station_serial,
                                        const Datetime &datetime_at_station) const {
            const TrainGroupSegment &from_station_segment =
                train_group_segments.get(group_train_info.segment_pointer, station_serial);
            return (datetime_at_station - from_station_segment.arrival_time.to_minutes()).getDate();
        }

        // Datetime get_departure_datetime(const TrainGroup &train_group_info, const int &station_serial,
        //                                 const Date &first_departure_date) const {
        //     const TrainGroupSegment &from_station_segment =
        //         train_group_segments.get(train_group_info.segment_pointer, station_serial);
        //     return Datetime(first_departure_date) + from_station_segment.departure_time;
        // }

        // Datetime get_arrival_datetime(const TrainGroup &train_group_info, const int &station_serial,
        //                               const Date &first_departure_date) const {
        //     const TrainGroupSegment &to_station_segment =
        //         train_group_segments.get(train_group_info.segment_pointer, station_serial);
        //     return Datetime(first_departure_date) + to_station_segment.arrival_time;
        // }

        struct TrainRange {
            train_id_t train_id;
            Datetime from_time;
            int from_station_serial;
            Datetime to_time;
            int to_station_serial;
        };

        norb::vector<TrainRange> query_ticket(const station_id_t &from_station_id, const station_id_t &to_station_id,
                                              const Datetime &datetime,
                                              const std::optional<train_group_id_t> &except = std::nullopt) const {
            norb::vector<TrainRange> results;
            // Step 1: Get the train groups from the lookup table
            const auto all_candidate_train_groups =
                station_train_group_lookup_store.find_all({from_station_id, to_station_id});
            // Step 2: Iterate through all_candidate_train_groups to verify them
            for (const auto &candidate_train_group : all_candidate_train_groups) {
                interface::log.as(LogLevel::DEBUG) << "Checking train group " << candidate_train_group.train_group_id
                                                   << " in range: [" << candidate_train_group.station_from_serial
                                                   << ", " << candidate_train_group.station_to_serial << "]\n";
                // Check if the train group is available on the given datetime
                const auto arrival_datetime_range = get_departure_datetime_range(
                    candidate_train_group.train_group_id, candidate_train_group.station_from_serial);
                if (except.has_value() &&
                    except.value() == candidate_train_group.train_group_id) {
                    interface::log.as(LogLevel::DEBUG)
                        << "Skipped because train group is in the except list.\n";
                    continue; // Skip this train group
                }
                if (not arrival_datetime_range.contains(datetime)) {
                    interface::log.as(LogLevel::DEBUG)
                        << "Skipped because train group is not available on the given date.\n";
                    continue; // Not available on this date
                }
                const auto &train_group_info =
                    train_group_store.find_first(candidate_train_group.train_group_id).value();
                const auto fist_departure_date =
                    deduce_departure_date_from(train_group_info, candidate_train_group.station_from_serial, datetime);
                const auto &train_id = train_id_t(candidate_train_group.train_group_id, fist_departure_date);
                interface::log.as(LogLevel::DEBUG) << "Registering train ID: " << train_id << '\n';

                const auto from_station_info = get_train_group_segment(train_group_info.segment_pointer,
                                                                       candidate_train_group.station_from_serial);
                const auto to_station_info =
                    get_train_group_segment(train_group_info.segment_pointer, candidate_train_group.station_to_serial);
                results.emplace_back(norb::Pair{candidate_train_group.train_group_id, fist_departure_date},
                                     Datetime(fist_departure_date) + from_station_info.departure_time,
                                     candidate_train_group.station_from_serial,
                                     Datetime(fist_departure_date) + to_station_info.arrival_time,
                                     candidate_train_group.station_to_serial);
            }
            return results;
        }
    };
} // namespace ticket
