// this file implements all user interface wrappers
#pragma once

#include <iostream>

#include "utility/io.hpp"

namespace ticket {

    struct TicketSystemStandardInterface {
        inline static auto &out = timestamped_cout;
        inline static auto &in = std::cin;
        inline static auto &log = no_logger;

        static void set_timestamp(int timestamp) {
            out.set_timestamp(timestamp);
            norb::Logger::set_line_number(timestamp);
        }
    };

    struct TicketSystemDebugInterface {
      private:
        inline static int timestamp = 0;

      public:
        inline static auto &out = timestamped_cout;
        inline static auto &in = std::cin;
        inline static auto &log = verbose_logger;

        static void set_timestamp(int timestamp) {
            TicketSystemDebugInterface::timestamp = timestamp;
            out.set_timestamp(timestamp);
            norb::Logger::set_line_number(timestamp);
        }

        static int get_timestamp() {
            return timestamp;
        }
    };
} // namespace ticket