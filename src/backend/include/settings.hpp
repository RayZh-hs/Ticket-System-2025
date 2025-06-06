#pragma once
#include "interface.hpp"
#include <utils.hpp>

namespace ticket {
    inline constexpr int max_bytes_per_chinese_char = 4;
    inline constexpr int max_username_length = 20;
    inline constexpr int max_password_length = 30;
    inline constexpr int min_password_length = 1;
    inline constexpr int max_name_characters = 5;
    inline constexpr int min_name_characters = 2;
    inline constexpr int max_mail_addr_length = 30;
    inline constexpr int max_train_id_length = 20;
    inline constexpr int max_station_num = 100;
    inline constexpr int max_station_name_characters = 10;

    inline constexpr const char train_group_segments_name[] = "train_group.segments";
    inline constexpr const char train_fare_segments_name[] = "train_fare.segments";

    using global_hash_method = norb::hash::Fnv1a64Hash;
    using global_interface = TicketSystemStandardInterface;
} // namespace ticket
