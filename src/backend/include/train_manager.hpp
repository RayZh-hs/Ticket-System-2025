#pragma once

#include "stlite/fixed_string.hpp"
#include "stlite/pair.hpp"
#include "stlite/range.hpp"

#include "settings.hpp"

namespace ticket {
    struct Train
    {
    public:
        using train_id_t = norb::FixedString<max_train_id_length>;
        using station_num_t = int;
        using station_name_t = norb::FixedUTF8String<max_bytes_per_chinese_char * max_station_name_characters>;
        using seat_num_t = int;
        using price_t = int;
        using timespan_t = int;
        using time_t = norb::Pair<int, int>;    // hh:mm
        using date_t = norb::Pair<int, int>;    // MM:dd
        using sale_date_t = norb::Range<date_t>;
        using train_type_t = char;

        train_id_t train_id;
        station_num_t station_num;
        station_name_t station_names[max_station_num];
        seat_num_t seat_num;
        price_t prices[max_station_num];
        time_t start_time;
        timespan_t travel_times[max_station_num];
        timespan_t stopover_times[max_station_num];
        sale_date_t sale_date;
        train_type_t type;
    };

    class TrainManager
    {
    public:
    };
}