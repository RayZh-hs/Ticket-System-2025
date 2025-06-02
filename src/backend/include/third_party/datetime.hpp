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
                assert(month >= 1 && month <= 12 && "Parsed month out of range");
                if (month >= 1 && month <= 12) {
                    assert(day >= 1 && day <= impl::DAYS_IN_MONTH_DATA[month] && "Parsed day out of range for month");
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

            Date operator+(const Date &delta) const {
                Date result = *this;
                result.month += delta.month;
                result.day += delta.day;

                if (result.month >= 1 && result.month <= 12) {
                    while (result.day > impl::DAYS_IN_MONTH_DATA[result.month]) {
                        result.day -= impl::DAYS_IN_MONTH_DATA[result.month];
                        result.month++;
                        if (result.month > 12) {
                            result.month = 1;
                        }
                    }
                    while (result.day < 1) {
                        result.month--;
                        if (result.month < 1) {
                            result.month = 12;
                        }
                        result.day += impl::DAYS_IN_MONTH_DATA[result.month];
                    }
                }
                assert(result.month >= 1 && result.month <= 12 && "Month out of range after Date::operator+");
                if (result.month >= 1 && result.month <= 12) {
                    assert(result.day >= 1 && result.day <= impl::DAYS_IN_MONTH_DATA[result.month] &&
                           "Day out of range after Date::operator+");
                }
                return result;
            }

            Date &operator+=(const Date &delta) {
                *this = *this + delta;
                return *this;
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
                assert(hour >= 0 && hour <= 23 && "Parsed hour out of range");
                assert(minute >= 0 && minute <= 59 && "Parsed minute out of range");
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

        void _update_components_from_total_minutes() {
            assert(total_minutes_since_epoch >= 0 && total_minutes_since_epoch < impl::TOTAL_MINUTES_IN_YEAR);

            int m = 1;
            for (m = 1; m <= 12; ++m) {
                if (total_minutes_since_epoch < impl::CUMULATIVE_MINUTES_HOLDER.values[m + 1]) {
                    break;
                }
            }
            this->date.month = m;

            int minutes_into_month = total_minutes_since_epoch - impl::CUMULATIVE_MINUTES_HOLDER.values[m];
            this->date.day = minutes_into_month / impl::MINUTES_PER_DAY + 1;

            int minutes_into_day = minutes_into_month % impl::MINUTES_PER_DAY;
            this->time.hour = minutes_into_day / impl::MINUTES_PER_HOUR;
            this->time.minute = minutes_into_day % impl::MINUTES_PER_HOUR;
        }

      public:
        Date date;
        Time time;

        Datetime(int m, int d, int h, int mn) {
            if (m < 1 || m > 12 || d < 1 || (m >= 1 && m <= 12 && d > impl::DAYS_IN_MONTH_DATA[m]) || h < 0 || h > 23 ||
                mn < 0 || mn > 59) {
                throw std::out_of_range("Invalid date/time components for Datetime constructor");
            }

            total_minutes_since_epoch = impl::CUMULATIVE_MINUTES_HOLDER.values[m] + (d - 1) * impl::MINUTES_PER_DAY +
                                        h * impl::MINUTES_PER_HOUR + mn;

            if (total_minutes_since_epoch < 0)
                throw time_underflow_error();
            if (total_minutes_since_epoch >= impl::TOTAL_MINUTES_IN_YEAR)
                throw time_overflow_error();

            this->date.month = m;
            this->date.day = d;
            this->time.hour = h;
            this->time.minute = mn;
        }

        Datetime(const Date &d_val, const Time &t_val) : Datetime(d_val.month, d_val.day, t_val.hour, t_val.minute) {
        }

        Datetime() : total_minutes_since_epoch(0) {
            _update_components_from_total_minutes();
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

            if (total_minutes_since_epoch < 0)
                throw time_underflow_error();
            if (total_minutes_since_epoch >= impl::TOTAL_MINUTES_IN_YEAR)
                throw time_overflow_error();

            _update_components_from_total_minutes();
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
            if (total_minutes < 0)
                throw time_underflow_error();
            if (total_minutes >= impl::TOTAL_MINUTES_IN_YEAR)
                throw time_overflow_error();

            Datetime dt;
            dt.total_minutes_since_epoch = total_minutes;
            dt._update_components_from_total_minutes();
            return dt;
        }

        void normalize() {
            if (total_minutes_since_epoch < 0) {
                throw time_underflow_error();
            }
            if (total_minutes_since_epoch >= impl::TOTAL_MINUTES_IN_YEAR) {
                throw time_overflow_error();
            }
            _update_components_from_total_minutes();
        }

        Datetime operator+(const Datetime &other) const {
            Datetime result;
            result.total_minutes_since_epoch = this->total_minutes_since_epoch + other.total_minutes_since_epoch;
            result.normalize();
            return result;
        }

        Datetime operator-(const Datetime &other) const {
            Datetime result;
            result.total_minutes_since_epoch = this->total_minutes_since_epoch - other.total_minutes_since_epoch;
            result.normalize();
            return result;
        }

        Datetime &operator+=(const Datetime &other) {
            this->total_minutes_since_epoch += other.total_minutes_since_epoch;
            this->normalize();
            return *this;
        }

        Datetime &operator-=(const Datetime &other) {
            this->total_minutes_since_epoch -= other.total_minutes_since_epoch;
            this->normalize();
            return *this;
        }

        [[nodiscard]] Datetime diff(const Datetime &other) const {
            Datetime result;
            result.total_minutes_since_epoch = this->total_minutes_since_epoch - other.total_minutes_since_epoch;
            result.normalize();
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
            return static_cast<std::string>(date) + " " + static_cast<std::string>(time);
        }

        static Datetime max() {
            return Datetime(12, 31, 23, 59);
        }

        static Datetime min() {
            return Datetime(1, 1, 0, 0);
        }

        friend std::ostream &operator<<(std::ostream &os, const Datetime &dt) {
            return os << static_cast<std::string>(dt);
        }
    };

    inline Datetime::Date::operator Datetime() const {
        return Datetime::from_date(month, day);
    }
    inline Datetime::Time::operator Datetime() const {
        return Datetime::from_time(hour, minute);
    }

    using DeltaDatetime = Datetime;

} // namespace norb