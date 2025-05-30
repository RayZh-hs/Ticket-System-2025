#include "datetime.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept> // For testing potential exceptions from semantic_cast
#include <string>

// Helper for printing test section headers
void test_section(const std::string &name) {
    std::cout << "\n--- Testing " << name << " ---" << std::endl;
}

// Helper for checking conditions and printing messages
void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        // Optionally, throw or exit to stop on first failure
        // throw std::runtime_error("Test failed: " + message);
    }
    assert(condition && message.c_str()); // assert will halt if condition is false
    // If asserts are disabled (NDEBUG), the cerr message above will still print.
    // std::cout << "PASS: " << message << std::endl; // Optional: print pass messages
}

// Helper to compare Datetime objects and print them if they don't match
void check_dt_eq(const norb::Datetime &dt, int m, int d, int h, int mn, const std::string &msg) {
    norb::Datetime expected(m, d, h, mn);
    if (dt != expected) {
        std::cerr << "FAIL: " << msg << std::endl;
        std::cerr << "      Expected: " << static_cast<std::string>(expected)
                  << ", Got: " << static_cast<std::string>(dt) << std::endl;
    }
    assert(dt == expected && msg.c_str());
}

int main() {
    using namespace norb;

    test_section("Constructors and Basic Accessors");
    {
        Datetime dt_default;
        check_dt_eq(dt_default, 0, 0, 0, 0, "Default constructor");

        Datetime dt_params(3, 15, 10, 30);
        check_dt_eq(dt_params, 3, 15, 10, 30, "Params constructor (M,D,H,MN)");
        check(dt_params.date.month == 3 && dt_params.date.day == 15, "Date part access");
        check(dt_params.time.hour == 10 && dt_params.time.minute == 30, "Time part access");

        Datetime::Date date_obj_params(4, 20);
        check(date_obj_params.month == 4 && date_obj_params.day == 20, "Date obj params constructor");
        Datetime::Time time_obj_params(14, 45);
        check(time_obj_params.hour == 14 && time_obj_params.minute == 45, "Time obj params constructor");

        Datetime dt_date_time(date_obj_params, time_obj_params);
        check_dt_eq(dt_date_time, 4, 20, 14, 45, "Datetime from Date and Time objects");

        Datetime dt_str("05-25 16:50");
        check_dt_eq(dt_str, 5, 25, 16, 50, "Datetime from string");

        Datetime::Date date_str("06-10");
        check(date_str.month == 6 && date_str.day == 10, "Date from string");
        Datetime::Time time_str("18:20");
        check(time_str.hour == 18 && time_str.minute == 20, "Time from string");

        Datetime dt_f_date = Datetime::from_date(7, 5);
        check_dt_eq(dt_f_date, 7, 5, 0, 0, "Static from_date");
        Datetime dt_f_time = Datetime::from_time(20, 55);
        check_dt_eq(dt_f_time, 0, 0, 20, 55, "Static from_time");
        Datetime dt_f_dt = Datetime::from_datetime(8, 12, 22, 10);
        check_dt_eq(dt_f_dt, 8, 12, 22, 10, "Static from_datetime");
    }

    test_section("String Conversions");
    {
        Datetime dt(10, 5, 8, 15);
        check(static_cast<std::string>(dt) == "10-05 08:15", "Datetime to string");
        check(static_cast<std::string>(dt.date) == "10-05", "Date to string");
        check(static_cast<std::string>(dt.time) == "08:15", "Time to string");

        Datetime dt_single_digit(1, 2, 3, 4);
        check(static_cast<std::string>(dt_single_digit) == "01-02 03:04", "Datetime to string (single digits)");
    }

    test_section("Object Conversions to Datetime");
    {
        Datetime::Date date_conv(11, 20);
        auto dt_from_date_op = Datetime(date_conv); // Uses Date::operator Datetime()
        check_dt_eq(dt_from_date_op, 11, 20, 0, 0, "Date object to Datetime conversion");

        Datetime::Time time_conv(21, 35);
        auto dt_from_time_op = Datetime(time_conv); // Uses Time::operator Datetime()
        check_dt_eq(dt_from_time_op, 0, 0, 21, 35, "Time object to Datetime conversion");
    }

    test_section("Arithmetic: Addition and Normalization");
    {
        Datetime base(1, 1, 10, 0); // Jan 1, 10:00
        check_dt_eq(base + DeltaDatetime(0, 0, 0, 30), 1, 1, 10, 30, "Add 30 minutes");
        check_dt_eq(base + DeltaDatetime(0, 0, 2, 0), 1, 1, 12, 0, "Add 2 hours");
        check_dt_eq(base + DeltaDatetime(0, 1, 0, 0), 1, 2, 10, 0, "Add 1 day");
        check_dt_eq(base + DeltaDatetime(1, 0, 0, 0), 2, 1, 10, 0, "Add 1 month");

        // Minute overflow
        Datetime d_min_ovf(1, 1, 0, 50);
        check_dt_eq(d_min_ovf + DeltaDatetime(0, 0, 0, 20), 1, 1, 1, 10, "Minute overflow (50m + 20m)");

        // Hour overflow
        Datetime d_hr_ovf(1, 1, 23, 0);
        check_dt_eq(d_hr_ovf + DeltaDatetime(0, 0, 2, 0), 1, 2, 1, 0, "Hour overflow (23h + 2h)");

        // Day overflow (to next month)
        Datetime d_day_ovf_jan(1, 30, 0, 0); // Jan 30
        check_dt_eq(d_day_ovf_jan + DeltaDatetime(0, 2, 0, 0), 2, 1, 0, 0, "Day overflow (Jan 30 + 2 days -> Feb 1)");
        Datetime d_day_ovf_feb(2, 27, 0, 0); // Feb 27 (2025 is not leap)
        check_dt_eq(d_day_ovf_feb + DeltaDatetime(0, 2, 0, 0), 3, 1, 0, 0, "Day overflow (Feb 27 + 2 days -> Mar 1)");

        // Month overflow (wraps around, year fixed)
        Datetime d_mon_ovf_dec(12, 1, 0, 0); // Dec 1
        check_dt_eq(d_mon_ovf_dec + DeltaDatetime(1, 0, 0, 0), 1, 1, 0, 0, "Month overflow (Dec + 1 month -> Jan)");
        Datetime d_mon_ovf_nov(11, 1, 0, 0); // Nov 1
        check_dt_eq(d_mon_ovf_nov + DeltaDatetime(2, 0, 0, 0), 1, 1, 0, 0, "Month overflow (Nov + 2 months -> Jan)");

        // Complex normalization (multiple carries)
        Datetime dt_c1(1, 31, 23, 50); // Jan 31, 23:50
        check_dt_eq(dt_c1 + DeltaDatetime(0, 0, 0, 20), 2, 1, 0, 10,
                    "Complex add: Jan 31, 23:50 + 20min -> Feb 1, 00:10");
        Datetime dt_c2(12, 31, 23, 59); // Dec 31, 23:59
        check_dt_eq(dt_c2 + DeltaDatetime(0, 0, 0, 1), 1, 1, 0, 0, "Complex add: Dec 31, 23:59 + 1min -> Jan 1, 00:00");
        Datetime dt_c3(1, 1, 0, 0);
        check_dt_eq(
            dt_c3 + DeltaDatetime(0, 60, 0, 0), 3, 2, 0, 0,
            "Complex add: Jan 1 + 60 days -> Mar 2 (Jan:31, Feb:28 -> 31+28=59. 60-59=1. So Mar 1 + 1 day = Mar 2)");
        // Jan 1 + 31 days = Feb 1. Remaining 29 days.
        // Feb 1 + 28 days = Mar 1. Remaining 1 day.
        // Mar 1 + 1 day = Mar 2. Correct.
    }

    test_section("Arithmetic: Subtraction and Normalization");
    {
        Datetime base(3, 15, 10, 30); // Mar 15, 10:30
        check_dt_eq(base - DeltaDatetime(0, 0, 0, 10), 3, 15, 10, 20, "Subtract 10 minutes");
        check_dt_eq(base - DeltaDatetime(0, 0, 2, 0), 3, 15, 8, 30, "Subtract 2 hours");
        check_dt_eq(base - DeltaDatetime(0, 1, 0, 0), 3, 14, 10, 30, "Subtract 1 day");
        check_dt_eq(base - DeltaDatetime(1, 0, 0, 0), 2, 15, 10, 30, "Subtract 1 month");

        // Minute borrow
        Datetime d_min_brw(3, 15, 10, 0);
        check_dt_eq(d_min_brw - DeltaDatetime(0, 0, 0, 10), 3, 15, 9, 50, "Minute borrow (10:00 - 10m)");

        // Hour borrow
        Datetime d_hr_brw(3, 15, 0, 0);
        check_dt_eq(d_hr_brw - DeltaDatetime(0, 0, 1, 0), 3, 14, 23, 0, "Hour borrow (00:00 - 1h)");

        // Day borrow (from previous month)
        Datetime d_day_brw_mar(3, 1, 0, 0); // Mar 1
        check_dt_eq(d_day_brw_mar - DeltaDatetime(0, 1, 0, 0), 2, 28, 0, 0, "Day borrow (Mar 1 - 1 day -> Feb 28)");
        Datetime d_day_brw_feb(2, 1, 0, 0); // Feb 1
        check_dt_eq(d_day_brw_feb - DeltaDatetime(0, 1, 0, 0), 1, 31, 0, 0, "Day borrow (Feb 1 - 1 day -> Jan 31)");
        Datetime d_day_brw_jan(1, 1, 0, 0); // Jan 1
        check_dt_eq(d_day_brw_jan - DeltaDatetime(0, 1, 0, 0), 12, 31, 0, 0, "Day borrow (Jan 1 - 1 day -> Dec 31)");

        // Month borrow (wraps around, year fixed)
        Datetime d_mon_brw_jan(1, 15, 0, 0); // Jan 15
        check_dt_eq(d_mon_brw_jan - DeltaDatetime(1, 0, 0, 0), 12, 15, 0, 0, "Month borrow (Jan - 1 month -> Dec)");
        Datetime d_mon_brw_feb(2, 15, 0, 0); // Feb 15
        check_dt_eq(d_mon_brw_feb - DeltaDatetime(2, 0, 0, 0), 12, 15, 0, 0, "Month borrow (Feb - 2 months -> Dec)");

        // Complex normalization (multiple borrows)
        Datetime dt_c1(3, 1, 0, 10); // Mar 1, 00:10
        check_dt_eq(dt_c1 - DeltaDatetime(0, 0, 0, 20), 2, 28, 23, 50,
                    "Complex sub: Mar 1, 00:10 - 20min -> Feb 28, 23:50");
        Datetime dt_c2(1, 1, 0, 0); // Jan 1, 00:00
        check_dt_eq(dt_c2 - DeltaDatetime(0, 0, 0, 1), 12, 31, 23, 59,
                    "Complex sub: Jan 1, 00:00 - 1min -> Dec 31, 23:59");
        Datetime dt_c3(1, 1, 0, 0); // Jan 1
        check_dt_eq(dt_c3 - DeltaDatetime(0, 35, 0, 0), 11, 27, 0, 0, "Complex sub: Jan 1 - 35 days -> Nov 27");
        // Jan 1 (-1 day) -> Dec 31. (34 days left)
        // Dec 31 (-31 days) -> Nov 30. (3 days left)
        // Nov 30 (-3 days) -> Nov 27. Correct.
    }

    test_section("Arithmetic: Diff operation");
    {
        Datetime dt_a(3, 15, 10, 30);
        Datetime dt_b(2, 10, 8, 20);
        DeltaDatetime delta_ab = dt_a.diff(dt_b);
        check(delta_ab.date.month == 1 && delta_ab.date.day == 5 && delta_ab.time.hour == 2 &&
                  delta_ab.time.minute == 10,
              "Diff test (a-b)");
        check(dt_b + delta_ab == dt_a, "Diff and add back (b + (a-b) == a)");

        Datetime dt_c(2, 10, 8, 20);
        Datetime dt_d(3, 15, 10, 30);
        DeltaDatetime delta_cd = dt_c.diff(dt_d); // Results in negative components
        check(delta_cd.date.month == -1 && delta_cd.date.day == -5 && delta_cd.time.hour == -2 &&
                  delta_cd.time.minute == -10,
              "Diff test (c-d)");
        check(dt_d + delta_cd == dt_c, "Diff and add back (d + (c-d) == c)");
    }

    test_section("Comparison Operators");
    {
        Datetime d1(3, 15, 10, 30);
        Datetime d2(3, 15, 10, 30); // Equal to d1
        Datetime d3(3, 15, 10, 31); // Minute greater
        Datetime d4(3, 15, 11, 30); // Hour greater
        Datetime d5(3, 16, 10, 30); // Day greater
        Datetime d6(4, 15, 10, 30); // Month greater

        check(d1 == d2, "Equality == (equal)");
        check(!(d1 == d3), "Equality == (unequal)");
        check(d1 != d3, "Inequality != (unequal)");
        check(!(d1 != d2), "Inequality != (equal)");

        check(d1 < d3, "Less than < (minute)");
        check(d1 < d4, "Less than < (hour)");
        check(d1 < d5, "Less than < (day)");
        check(d1 < d6, "Less than < (month)");
        check(!(d3 < d1), "Less than < (false)");
        check(!(d1 < d2), "Less than < (equal)");

        check(d1 <= d2, "Less or equal <= (equal)");
        check(d1 <= d3, "Less or equal <= (less)");
        check(!(d3 <= d1), "Less or equal <= (false)");

        check(d3 > d1, "Greater than > (minute)");
        check(d4 > d1, "Greater than > (hour)");
        check(d5 > d1, "Greater than > (day)");
        check(d6 > d1, "Greater than > (month)");
        check(!(d1 > d3), "Greater than > (false)");
        check(!(d1 > d2), "Greater than > (equal)");

        check(d2 >= d1, "Greater or equal >= (equal)");
        check(d3 >= d1, "Greater or equal >= (greater)");
        check(!(d1 >= d3), "Greater or equal >= (false)");
    }

    test_section("DeltaDatetime Alias");
    {
        DeltaDatetime delta; // Uses Datetime default constructor
        delta.date.day = 5;
        delta.time.hour = 2;
        Datetime start(1, 1, 0, 0);
        Datetime end = start + delta;
        check_dt_eq(end, 1, 6, 2, 0, "DeltaDatetime usage");
    }

    std::cout << "\nAll assertion checks passed!" << std::endl;
    return 0;
}