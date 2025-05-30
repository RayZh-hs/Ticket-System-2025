#pragma once

#include <utils.hpp>

namespace ticket {
    constexpr int max_bytes_per_chinese_char = 4;
    constexpr int max_username_length = 20;
    constexpr int max_password_length = 30;
    constexpr int min_password_length = 1;
    constexpr int max_name_characters = 5;
    constexpr int min_name_characters = 2;
    constexpr int max_mail_addr_length = 30;
    constexpr int max_train_id_length = 20;
    constexpr int max_station_num = 100;
    constexpr int max_station_name_characters = 10;

    using global_hash_method = norb::hash::Djb2Hash;

    constexpr auto log_file_path = "latest.log";
} // namespace ticket
