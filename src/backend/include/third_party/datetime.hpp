#pragma once

#include "semantic_cast.hpp"
#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace norb {

    struct time_overflow_error : std::runtime_error {
        time_overflow_error() : std::runtime_error("Timestamp results in a date beyond 2025") {
        }
    };

    struct time_underflow_error : std::runtime_error {
        time_underflow_error() : std::runtime_error("Timestamp results in a date before 2025") {
        }
    };

    struct Datetime;

    namespace impl {
        static const int DAYS_IN_MONTH_DATA[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

        static constexpr int MINUTES_PER_HOUR = 60;
        static constexpr int HOURS_PER_DAY = 24;
        static constexpr int MINUTES_PER_DAY = MINUTES_PER_HOUR * HOURS_PER_DAY;
        static constexpr int TOTAL_DAYS_IN_YEAR = 365;
        static constexpr int TOTAL_MINUTES_IN_YEAR = TOTAL_DAYS_IN_YEAR * MINUTES_PER_DAY;

        struct CumulativeMinutesHolder {
            int values[14];

            CumulativeMinutesHolder() {
                values[0] = 0;
                values[1] = 0;
                for (int m = 2; m <= 12; ++m) {
                    values[m] = values[m - 1] + DAYS_IN_MONTH_DATA[m - 1] * MINUTES_PER_DAY;
                }
                values[13] = values[12] + DAYS_IN_MONTH_DATA[12] * MINUTES_PER_DAY;
                assert(values[13] == TOTAL_MINUTES_IN_YEAR);
            }
        };

        static const CumulativeMinutesHolder CUMULATIVE_MINUTES_HOLDER;

        inline std::string format_two_digits(int val) {
            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << val;
            return oss.str();
        }
    } // namespace impl

    struct Datetime {
      public:
        struct Date {
            int month, day;

            constexpr explicit Date(int m = 1, int d = 1) : month(m), day(d) {
            }

            explicit Date(const std::string &repr) {
                assert(repr.size() == 5 && repr[2] == '-');
                month = semantic_cast<int>(repr.substr(0, 2));
                day = semantic_cast<int>(repr.substr(3, 2));
                assert(month >= 1 && month <= 12);
                if (month >= 1 && month <= 12) {
                    assert(day >= 1 && day <= impl::DAYS_IN_MONTH_DATA[month]);
                }
            }

            explicit operator std::string() const {
                return impl::format_two_digits(month) + "-" + impl::format_two_digits(day);
            }

            bool operator==(const Date &other) const {
                return month == other.month && day == other.day;
            }

            bool operator!=(const Date &other) const {
                return !(*this == other);
            }

            auto operator<=>(const Date &other) const {
                if (month != other.month)
                    return month <=> other.month;
                return day <=> other.day;
            }

            explicit operator Datetime() const;

            static constexpr Date max() {
                return Date{12, 31};
            }
            static constexpr Date min() {
                return Date{1, 1};
            }

            Date &operator+=(const Date &delta) {
                month += delta.month;
                day += delta.day;

                // Normalize the date
                while (day > impl::DAYS_IN_MONTH_DATA[month]) {
                    day -= impl::DAYS_IN_MONTH_DATA[month];
                    month++;
                    if (month > 12) {
                        month = 1;
                    }
                }
                return *this;
            }

            Date operator+(const Date &delta) const {
                Date result = *this;
                result += delta;
                return result;
            }

            Date &operator+=(const int days) {
                day += days;

                // Normalize the date
                while (day > impl::DAYS_IN_MONTH_DATA[month]) {
                    day -= impl::DAYS_IN_MONTH_DATA[month];
                    month++;
                    if (month > 12) {
                        month = 1;
                    }
                }
                return *this;
            }

            Date operator+(const int days) const {
                Date result = *this;
                result += days;
                return result;
            }

            Date &operator++() { // prefix increment
                day++;
                if (day > impl::DAYS_IN_MONTH_DATA[month]) {
                    day = 1;
                    month++;
                    if (month > 12) {
                        month = 1;
                    }
                }
                return *this;
            }

            Date operator++(int) { // postfix increment
                Date temp = *this;
                ++(*this);
                return temp;
            }

            Date operator-(const int &days) const {
                Date result = *this;
                result.day -= days;

                // Normalize the date
                while (result.day <= 0) {
                    result.month--;
                    if (result.month < 1) {
                        result.month = 12;
                    }
                    result.day += impl::DAYS_IN_MONTH_DATA[result.month];
                }
                return result;
            }
        };

        struct Time {
            int hour, minute;

            constexpr explicit Time(int h = 0, int m = 0) : hour(h), minute(m) {
            }

            explicit Time(const std::string &repr) {
                assert(repr.size() == 5 && repr[2] == ':');
                hour = semantic_cast<int>(repr.substr(0, 2));
                minute = semantic_cast<int>(repr.substr(3, 2));
                assert(hour >= 0 && hour <= 23);
                assert(minute >= 0 && minute <= 59);
            }

            bool operator==(const Time &other) const {
                return hour == other.hour && minute == other.minute;
            }

            auto operator<=>(const Time &other) const {
                if (hour != other.hour)
                    return hour <=> other.hour;
                return minute <=> other.minute;
            }

            explicit operator std::string() const {
                return impl::format_two_digits(hour) + ":" + impl::format_two_digits(minute);
            }

            explicit operator Datetime() const;

            static constexpr Time max() {
                return Time{23, 59};
            }
            static constexpr Time min() {
                return Time{0, 0};
            }
        };

      private:
        int total_minutes_since_epoch;

        void _check_bounds_and_throw() {
            if (total_minutes_since_epoch < 0) {
                throw time_underflow_error();
            }
            if (total_minutes_since_epoch >= impl::TOTAL_MINUTES_IN_YEAR) {
                throw time_overflow_error();
            }
        }

      public:
        Datetime(int m, int d, int h, int mn) {
            if (m < 1 || m > 12 || d < 1 || (m >= 1 && m <= 12 && d > impl::DAYS_IN_MONTH_DATA[m]) || h < 0 || h > 23 ||
                mn < 0 || mn > 59) {
                throw std::out_of_range("Invalid date/time components for Datetime constructor");
            }

            total_minutes_since_epoch = impl::CUMULATIVE_MINUTES_HOLDER.values[m] + (d - 1) * impl::MINUTES_PER_DAY +
                                        h * impl::MINUTES_PER_HOUR + mn;
            _check_bounds_and_throw();
        }

        Datetime(const Date &d_val, const Time &t_val) : Datetime(d_val.month, d_val.day, t_val.hour, t_val.minute) {
        }

        Datetime() : total_minutes_since_epoch(0) {
        }

        explicit Datetime(const std::string &repr) {
            assert(repr.size() == 11 && repr[5] == ' ');
            Date d_parsed(repr.substr(0, 5));
            Time t_parsed(repr.substr(6, 5));

            if (d_parsed.month < 1 || d_parsed.month > 12 || d_parsed.day < 1 ||
                (d_parsed.month >= 1 && d_parsed.month <= 12 &&
                 d_parsed.day > impl::DAYS_IN_MONTH_DATA[d_parsed.month]) ||
                t_parsed.hour < 0 || t_parsed.hour > 23 || t_parsed.minute < 0 || t_parsed.minute > 59) {
                throw std::out_of_range("Invalid date/time components from string for Datetime constructor");
            }

            total_minutes_since_epoch = impl::CUMULATIVE_MINUTES_HOLDER.values[d_parsed.month] +
                                        (d_parsed.day - 1) * impl::MINUTES_PER_DAY +
                                        t_parsed.hour * impl::MINUTES_PER_HOUR + t_parsed.minute;
            _check_bounds_and_throw();
        }

        static Datetime from_date(int m, int d) {
            return Datetime(m, d, 0, 0);
        }
        static Datetime from_time(int h, int mn) {
            return Datetime(1, 1, h, mn);
        }
        static Datetime from_datetime(int m, int d, int h, int mn) {
            return Datetime(m, d, h, mn);
        }
        static Datetime from_minutes(int total_minutes) {
            Datetime dt;
            dt.total_minutes_since_epoch = total_minutes;
            dt._check_bounds_and_throw();
            return dt;
        }

        int to_minutes() const {
            return total_minutes_since_epoch;
        }

        int to_days() const {
            return total_minutes_since_epoch / impl::MINUTES_PER_DAY;
        }

        int getMonth() const {
            assert(total_minutes_since_epoch >= 0 && total_minutes_since_epoch < impl::TOTAL_MINUTES_IN_YEAR);
            int m = 1;
            for (m = 1; m <= 12; ++m) {
                if (total_minutes_since_epoch < impl::CUMULATIVE_MINUTES_HOLDER.values[m + 1]) {
                    break;
                }
            }
            return m;
        }

        int getDay() const {
            assert(total_minutes_since_epoch >= 0 && total_minutes_since_epoch < impl::TOTAL_MINUTES_IN_YEAR);
            int m = getMonth();
            int minutes_into_month = total_minutes_since_epoch - impl::CUMULATIVE_MINUTES_HOLDER.values[m];
            return minutes_into_month / impl::MINUTES_PER_DAY + 1;
        }

        int getHour() const {
            assert(total_minutes_since_epoch >= 0 && total_minutes_since_epoch < impl::TOTAL_MINUTES_IN_YEAR);
            return (total_minutes_since_epoch % impl::MINUTES_PER_DAY) / impl::MINUTES_PER_HOUR;
        }

        int getMinute() const {
            assert(total_minutes_since_epoch >= 0 && total_minutes_since_epoch < impl::TOTAL_MINUTES_IN_YEAR);
            return total_minutes_since_epoch % impl::MINUTES_PER_HOUR;
        }

        Date getDate() const {
            return Date(getMonth(), getDay());
        }

        Time getTime() const {
            return Time(getHour(), getMinute());
        }

        Datetime operator+(const Datetime &other) const {
            Datetime result;
            result.total_minutes_since_epoch = this->total_minutes_since_epoch + other.total_minutes_since_epoch;
            result._check_bounds_and_throw();
            return result;
        }

        Datetime operator-(const Datetime &other) const {
            Datetime result;
            result.total_minutes_since_epoch = this->total_minutes_since_epoch - other.total_minutes_since_epoch;
            result._check_bounds_and_throw();
            return result;
        }

        Datetime &operator+=(const Datetime &other) {
            this->total_minutes_since_epoch += other.total_minutes_since_epoch;
            this->_check_bounds_and_throw();
            return *this;
        }

        Datetime &operator-=(const Datetime &other) {
            this->total_minutes_since_epoch -= other.total_minutes_since_epoch;
            this->_check_bounds_and_throw();
            return *this;
        }

        [[nodiscard]] Datetime diff(const Datetime &other) const {
            Datetime result;
            result.total_minutes_since_epoch = this->total_minutes_since_epoch - other.total_minutes_since_epoch;
            result._check_bounds_and_throw();
            return result;
        }

        bool operator==(const Datetime &other) const {
            return total_minutes_since_epoch == other.total_minutes_since_epoch;
        }

        bool operator!=(const Datetime &other) const {
            return !(*this == other);
        }

        auto operator<=>(const Datetime &other) const {
            return total_minutes_since_epoch <=> other.total_minutes_since_epoch;
        }

        explicit operator std::string() const {
            return static_cast<std::string>(getDate()) + " " + static_cast<std::string>(getTime());
        }

        static Datetime max() {
            return Datetime(12, 31, 23, 59);
        }

        static Datetime min() {
            return Datetime(1, 1, 0, 0);
        }
    };

    inline std::ostream &operator<<(std::ostream &os, const Datetime &dt) {
        return os << static_cast<std::string>(dt);
    }

    inline std::ostream &operator<<(std::ostream &os, const Datetime::Date &date) {
        os << static_cast<std::string>(date);
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const Datetime::Time &time) {
        os << static_cast<std::string>(time);
        return os;
    }

    inline Datetime::Date::operator Datetime() const {
        return Datetime::from_date(month, day);
    }
    inline Datetime::Time::operator Datetime() const {
        return Datetime::from_time(hour, minute);
    }

    using DeltaDatetime = Datetime;

} // namespace norb