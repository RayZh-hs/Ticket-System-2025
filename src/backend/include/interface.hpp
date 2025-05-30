// this file implements all user interface wrappers
#pragma once

#include <iostream>

#include "logging.hpp"
#include "settings.hpp"

namespace ticket {
    struct TicketSystemStandardInterface
    {
        inline static auto &out = std::cout;
        inline static auto &in = std::cin;
        inline static auto &log = std::clog;
    };

    struct TicketSystemDebugInterface
    {
        inline static auto &out = std::cout;
        inline static auto &in = std::cin;
        inline static auto log = norb::logger(log_file_path);
    };
}