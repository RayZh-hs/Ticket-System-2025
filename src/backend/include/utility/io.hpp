#pragma once

#include <iostream>
#include "logging.hpp"

namespace ticket {

    inline extern const char log_file_path[] = "/home/rayzh/learn/sjtu/acm/proj/Ticket-System-2025/testcases/z.latest.log";
    inline auto verbose_logger = norb::Logger(log_file_path);
    inline auto no_logger = norb::NoLogger("");

    class CoutTimestampWrapper {
        inline static int timestamp = 0;

      public:
        CoutTimestampWrapper() = default;

        [[nodiscard]] auto &as() const {
            std::cout << '[' << timestamp << "] ";
            return std::cout;
        }

        [[nodiscard]] auto &as(std::nullptr_t) const {
            return std::cout;
        }

        void set_timestamp(const int &t) {
            timestamp = t;
        }
    };

    inline auto timestamped_cout = CoutTimestampWrapper();
} // namespace ticket