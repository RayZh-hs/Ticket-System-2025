#pragma once

#include "stlite/fixed_string.hpp"
#include "stlite/pair.hpp"
#include "stlite/range.hpp"
#include "b_plus_tree.hpp"
#include "datetime.hpp"

#include "settings.hpp"
#include "account_manager.hpp"

namespace ticket
{
    struct Train
    {
    public:
        using train_id_t = norb::FixedString<max_train_id_length>;
        using station_num_t = int;
        using station_name_t = norb::FixedUTF8String<max_bytes_per_chinese_char * max_station_name_characters>;
        using seat_num_t = int;
        using price_t = int;
        using train_type_t = char;

        train_id_t train_id;
        station_num_t station_num;
        station_name_t station_names[max_station_num];
        seat_num_t seat_num;
        price_t prices[max_station_num];
        norb::Datetime departure_times[max_station_num];
        norb::Datetime arrival_times[max_station_num];
        train_type_t type;

        bool has_been_released = false;
    };

    class TrainManager
    {
    public:
        using train_id_t = Train::train_id_t;
        using station_name_t = Train::station_name_t;
        using train_id_hash_t = global_hash_method::hash_t;
        using station_id_hash_t = global_hash_method::hash_t;

    private:
        norb::BPlusTree<train_id_hash_t, Train> train_info;
        norb::BPlusTree<norb::Pair<station_id_hash_t, norb::Datetime>, train_id_hash_t> station_info;
    public:
    };
}
