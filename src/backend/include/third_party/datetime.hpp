#pragma once

#include "semantic_cast.hpp"
#include <cassert>

namespace norb {

    struct time_overflow_error : std::runtime_error {
        time_overflow_error() : runtime_error("Timestamp beyond 2025") {
        }
    };

    struct time_underflow_error : std::runtime_error {
        time_underflow_error() : runtime_error("Timestamp before 2025") {
        }
    };

    struct Datetime {
      private:
        static std::string format_two_digits(int val) {
            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << val;
            return oss.str();
        }
        static constexpr int days_in_month_arr[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

      public:
        struct Date {
            int month, day;

            explicit Date(int m = 0, int d = 0) : month(m), day(d) {
            }

            explicit Date(const std::string &repr) {
                assert(repr.size() == 5 && repr[2] == '-');
                month = semantic_cast<int>(repr.substr(0, 2));
                day = semantic_cast<int>(repr.substr(3, 2));
            }

            explicit operator std::string() const {
                return format_two_digits(month) + "-" + format_two_digits(day);
            }

            explicit operator Datetime() const {
                return Datetime::from_date(month, day);
            }
        };

        struct Time {
            int hour, minute;

            explicit Time(int h = 0, int m = 0) : hour(h), minute(m) {
            }

            explicit Time(const std::string &repr) {
                assert(repr.size() == 5 && repr[2] == ':');
                hour = semantic_cast<int>(repr.substr(0, 2));
                minute = semantic_cast<int>(repr.substr(3, 2));
            }

            explicit operator std::string() const {
                return format_two_digits(hour) + ":" + format_two_digits(minute);
            }
            explicit operator Datetime() const {
                return Datetime::from_time(hour, minute);
            }
        };

        Date date;
        Time time;

        Datetime(int m, int d, int h, int mn) : date(m, d), time(h, mn) {
        }
        Datetime(const Date &d_val, const Time &t_val) : date(d_val), time(t_val) {
        }

        Datetime() : date(0, 0), time(0, 0) {
        }

        explicit Datetime(const std::string &repr) : date(repr.substr(0, 5)), time(repr.substr(6, 5)) {
            assert(repr.size() == 11 && repr[5] == ' ');
        }

        static Datetime from_date(int m, int d) {
            return {m, d, 0, 0};
        }
        static Datetime from_time(int h, int mn) {
            return {0, 0, h, mn};
        }
        static Datetime from_datetime(int m, int d, int h, int mn) {
            return {m, d, h, mn};
        }

        void normalize() {
            if (time.minute >= 60 || time.minute < 0) {
                time.hour += time.minute / 60;
                time.minute %= 60;
                if (time.minute < 0) {
                    time.minute += 60;
                    time.hour--;
                }
            }

            if (time.hour >= 24 || time.hour < 0) {
                date.day += time.hour / 24;
                time.hour %= 24;
                if (time.hour < 0) {
                    time.hour += 24;
                    date.day--;
                }
            }

            auto get_effective_month_idx = [](int m_val) { return ((m_val - 1) % 12 + 12) % 12 + 1; };

            while (date.day <= 0) {
                date.month--;
                const int prev_month_idx = get_effective_month_idx(date.month);
                date.day += days_in_month_arr[prev_month_idx];
            }

            int current_month_idx = get_effective_month_idx(date.month);
            while (date.day > days_in_month_arr[current_month_idx]) {
                date.day -= days_in_month_arr[current_month_idx];
                date.month++;
                current_month_idx = get_effective_month_idx(date.month);
            }

            date.month = get_effective_month_idx(date.month);
        }

        Datetime operator+(const Datetime &delta) const {
            Datetime result = *this;
            result.date.month += delta.date.month;
            result.date.day += delta.date.day;
            result.time.hour += delta.time.hour;
            result.time.minute += delta.time.minute;
            result.normalize();
            return result;
        }

        Datetime operator-(const Datetime &delta) const {
            Datetime result = *this;
            result.date.month -= delta.date.month;
            result.date.day -= delta.date.day;
            result.time.hour -= delta.time.hour;
            result.time.minute -= delta.time.minute;
            result.normalize();
            return result;
        }

        [[nodiscard]] Datetime diff(const Datetime &other) const {
            return {this->date.month - other.date.month, this->date.day - other.date.day,
                    this->time.hour - other.time.hour, this->time.minute - other.time.minute};
        }

        bool operator==(const Datetime &other) const {
            return date.month == other.date.month && date.day == other.date.day && time.hour == other.time.hour &&
                   time.minute == other.time.minute;
        }

        bool operator!=(const Datetime &other) const {
            return !(*this == other);
        }

        auto operator<=>(const Datetime &other) const {
            if (date.month != other.date.month)
                return date.month <=> other.date.month;
            if (date.day != other.date.day)
                return date.day <=> other.date.day;
            if (time.hour != other.time.hour)
                return time.hour <=> other.time.hour;
            return time.minute <=> other.time.minute;
        }

        explicit operator std::string() const {
            return static_cast<std::string>(date) + " " + static_cast<std::string>(time);
        }

        static Datetime max() {
            return from_datetime(12, 31, 23, 59);
        }

        static Datetime min() {
            return from_datetime(1, 1, 0, 0);
        }
    };

    using DeltaDatetime = Datetime;
} // namespace norb
