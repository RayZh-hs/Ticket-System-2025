#pragma once

#include "account_manager.hpp"
#include "settings.hpp"
#include "sorted_view.hpp"
#include "ticket_manager.hpp"
#include "train_manager.hpp"
#include "utility/wrappers.hpp"

#include <variant>

#ifndef NDEBUG
#include <map>
#endif

namespace ticket {
    struct DatetimePlaceholder {};
    std::ostream &operator<<(std::ostream &os, const DatetimePlaceholder &) {
        os << "xx-xx xx:xx";
        return os;
    }
    inline constexpr DatetimePlaceholder datetime_placeholder{};

    class TicketSystem {
      public:
        struct TrainRideInfo; // forward declaration
      private:
        AccountManager account_manager_;
        TrainManager train_manager_;
        TicketManager ticket_manager_;

        using LogLevel = norb::LogLevel;
        using account_id_t = Account::id_t;
        using Date = norb::Datetime::Date;
        using interface = global_interface;
        using station_id_t = TrainManager::station_id_t;

#ifndef NDEBUG
        inline static std::map<train_group_id_t, std::string> db_train_group_lookup;
        inline static std::map<station_id_t, std::string> db_station_lookup;
#endif

        // when the first train is fixed, we ought to compare the time of arrival
        static std::optional<TrainRideInfo>
        find_best_between(const station_id_t &from_id, const station_id_t &to_id, const Datetime &datetime,
                          const std::string &sort_by, const std::optional<train_group_id_t> &except = std::nullopt) {
            auto &ticket_manager = get_instance().ticket_manager_;
            auto &train_manager = get_instance().train_manager_;

            if (from_id == to_id) {
                return std::nullopt;
            }
            // find all trains that leave at from at date and arrive in to
            const auto &train_query_result = train_manager.query_ticket(from_id, to_id, datetime, except);
            if (train_query_result.empty())
                return std::nullopt;
            // from ticket_manager retrieve the financial information
            norb::vector<TrainFareSegment> financial_info;
            norb::vector<int> duration;
            for (const auto &train_item : train_query_result) {
                financial_info.push_back(ticket_manager.get_price_seat_for_section(
                    train_item.train_id, train_item.from_station_serial, train_item.to_station_serial));
                duration.push_back((train_item.to_time.to_minutes()));
            }
            int best_cursor = -1;
            const int train_count = train_query_result.size();
            if (sort_by == "cost") {
                best_cursor = norb::make_supreme(train_count, [&financial_info](const int &a, const int &b) {
                    return financial_info[a].price < financial_info[b].price;
                });
            } else { // sort by time
                best_cursor = norb::make_supreme(
                    train_count, [&duration](const int &a, const int &b) { return duration[a] < duration[b]; });
            }
            return best_cursor == -1
                       ? std::nullopt
                       : std::make_optional<TrainRideInfo>(
                             {train_query_result[best_cursor].train_id, from_id, to_id,
                              train_query_result[best_cursor].from_time, train_query_result[best_cursor].to_time,
                              financial_info[best_cursor].price, financial_info[best_cursor].remaining_seats});
        }

      public:
        static TicketSystem &get_instance() {
            static TicketSystem ticket_manager;
            return ticket_manager;
        }

        static int add_user(const std::string &current_user, const std::string &username, const std::string &password,
                            const std::string &name, const std::string &mail_addr, const int &privilege) {
            auto &account_manager = get_instance().account_manager_;
            if (account_manager.count_registered_users() == 0) {
                global_interface::log.as(LogLevel::DEBUG) << "No current user, expected to be admin" << '\n';
                account_manager.add_user({
                    .username = username,
                    .hashed_password = Account::hash_password(password),
                    .name = name,
                    .mail_addr = mail_addr,
                    .privilege = 10,
                });
            } else {
                global_interface::log.as(LogLevel::DEBUG)
                    << "Before adding, user count: " << account_manager.count_registered_users() << '\n';
                const auto new_user_id = Account::id_from_username(username);
                if (account_manager.is_registered(new_user_id)) {
                    global_interface::log.as(LogLevel::WARNING)
                        << "Add user failed because user #" << new_user_id << " has been registered" << '\n';
                    return -1;
                }
                const auto cur_user_id = Account::id_from_username(current_user);
                const auto cur_user_account = account_manager.find_active_user(cur_user_id);
                if (not cur_user_account.has_value()) {
                    global_interface::log.as(LogLevel::WARNING)
                        << "Add user failed because user #" << cur_user_id << " is not logged in" << '\n';
                    return -1;
                }
                if (cur_user_account->privilege <= privilege) {
                    global_interface::log.as(LogLevel::WARNING)
                        << "Add user failed because current user privilege " << cur_user_account->privilege
                        << "< required " << privilege << '\n';
                    return -1;
                }
                // The command shall succeed
                account_manager.add_user({
                    .username = username,
                    .hashed_password = Account::hash_password(password),
                    .name = name,
                    .mail_addr = mail_addr,
                    .privilege = privilege,
                });
            }
            return 0; // Success
        }

        static int login(const std::string &username, const std::string &password) {
            auto &account_manager = get_instance().account_manager_;
            const auto account_id = Account::id_from_username(username);
            const auto hashed_password = Account::hash_password(password);
            global_interface::log.as(LogLevel::DEBUG) << "User " << username << "(#" << account_id << ")"
                                                      << " attempts login with HP=" << hashed_password << '\n';
            try {
                account_manager.login(account_id, hashed_password);
                interface::log.as(LogLevel::INFO) << "Login successful" << '\n';
                return 0;
            } catch (std::runtime_error &e) {
                global_interface::log.as(LogLevel::WARNING) << "Login failed: " << e.what() << '\n';
                return -1;
            }
        }

        static int logout(const std::string &username) {
            auto &account_manager = get_instance().account_manager_;
            const auto account_id = Account::id_from_username(username);
            global_interface::log.as(LogLevel::DEBUG) << "User #" << account_id << " attempts to logout" << '\n';
            try {
                account_manager.logout(account_id);
                return 0;
            } catch (std::runtime_error &e) {
                global_interface::log.as(LogLevel::WARNING) << "Logout failed: " << e.what() << '\n';
                return -1;
            }
        }

        static std::variant<int, Account> query_profile(const std::string &current_username,
                                                        const std::string &username) {
            auto account_manager = get_instance().account_manager_;
            const auto current_user_id = Account::id_from_username(current_username);
            const auto current_user_info = account_manager.find_active_user(current_user_id);
            if (not current_user_info.has_value()) {
                global_interface::log.as(LogLevel::WARNING) << current_username << " is not logged in" << '\n';
                return -1;
            }
            const auto account_id = Account::id_from_username(username);
            const auto account_info = account_manager.find_user(account_id);
            if (not account_info.has_value()) {
                global_interface::log.as(LogLevel::WARNING) << username << " is not registered" << '\n';
                return -1;
            }
            const auto current_user_privilege = current_user_info->privilege;
            const auto user_privilege = account_info->privilege;
            if ((user_privilege >= current_user_privilege) and (current_user_id != account_id)) {
                global_interface::log.as(LogLevel::WARNING)
                    << "QueryProfile failed because privilege underflow: " << current_user_privilege
                    << " <=" << user_privilege << '\n';
                return -1;
            }
            return account_info.value();
        }

        static std::variant<int, Account>
        modify_profile(const std::string &current_username, const std::string &username,
                       const std::optional<std::string> &password, const std::optional<std::string> &name,
                       const std::optional<std::string> &mail_addr, const std::optional<int> &privilege) {
            auto &account_manager = get_instance().account_manager_;
            const auto current_user_id = Account::id_from_username(current_username);
            const auto current_account_info = account_manager.find_active_user(current_user_id);
            if (not current_account_info.has_value()) {
                global_interface::log.as(LogLevel::WARNING)
                    << "ModifyProfile failed because current user " << current_username << " has not logged in" << '\n';
                return -1;
            }
            const auto account_id = Account::id_from_username(username);
            auto account_info = account_manager.find_user(account_id);
            if (not account_info.has_value()) {
                global_interface::log.as(LogLevel::WARNING)
                    << "ModifyProfile failed because " << username << " is not registered" << '\n';
                return -1;
            }
            if (not(current_user_id == account_id or current_account_info->privilege > account_info->privilege)) {
                global_interface::log.as(LogLevel::WARNING)
                    << "ModifyProfile failed because " << current_username << " is not authorized" << '\n';
                return -1;
            }
            if (privilege.has_value() and current_account_info->privilege <= privilege) {
                global_interface::log.as(LogLevel::WARNING)
                    << "ModifyProfile failed because the intended privilege is beyond the current user's" << '\n';
                return -1;
            }
            // Change the user's account info
            if (password.has_value()) {
                account_info->hashed_password = Account::hash_password(password.value());
            }
            if (name.has_value()) {
                account_info->name = name.value();
            }
            if (mail_addr.has_value()) {
                account_info->mail_addr = mail_addr.value();
            }
            if (privilege.has_value()) {
                account_info->privilege = privilege.value();
            }
            // Update the account info in the stores
            account_manager.change_account_info(account_id, account_info.value());
            return account_info.value();
        }

        static int add_train(const std::string &train_group_name, const int &station_num, const int &seat_num,
                             const ConcentratedString<std::string> &station_names,
                             const ConcentratedString<int> &prices, const norb::Datetime::Time &start_time,
                             const ConcentratedString<int> &travel_times, const ConcentratedString<int> &stopover_times,
                             const ConcentratedString<norb::Datetime::Date> &sale_date, const char &type) {
            auto &train_manager = get_instance().train_manager_;
            auto &ticket_manager = get_instance().ticket_manager_;

            try {
                if (train_manager.exists_train_group(TrainManager::train_group_id_from_name(train_group_name))) {
                    global_interface::log.as(LogLevel::WARNING)
                        << "Add train failed: train group " << train_group_name << " already exists" << '\n';
                    return -1;
                }
                const auto decoded_station_names = station_names.as_vector();
                const auto decoded_prices = prices.as_vector();
                const auto decoded_travel_times = travel_times.as_vector();
                const auto decoded_stopover_times = stopover_times.as_vector();
                // assertions for the input data
                assert(decoded_station_names.size() == station_num);
                assert(decoded_prices.size() == station_num - 1);
                assert(decoded_travel_times.size() == station_num - 1);
                assert(decoded_stopover_times.size() == station_num - 2);
                // debug info register
#ifndef NDEBUG
                db_train_group_lookup[TrainManager::train_group_id_from_name(train_group_name)] = train_group_name;
#endif
                // build the segments for the train manager
                std::vector<TrainGroupSegment> segments;
                auto delta_time = norb::DeltaDatetime(start_time);
                // record the stations
                for (int i = 0; i < station_num; ++i) {
                    Datetime arrival_time, departure_time;
                    if (i == 0) {
                        arrival_time = departure_time = delta_time;
                    } else {
                        arrival_time = (delta_time += Datetime::from_minutes(decoded_travel_times[i - 1]));
                        if (i < station_num - 1) {
                            departure_time = (delta_time += Datetime::from_minutes(decoded_stopover_times[i - 1]));
                        }
                    }
                    global_interface::log.as(LogLevel::DEBUG)
                        << "Adding segment for station " << decoded_station_names[i] << ": " << "arrival at "
                        << arrival_time << ", departure at " << departure_time << '\n';
                    train_manager.register_station(decoded_station_names[i]);
                    const auto station_id = TrainManager::station_id_from_name(decoded_station_names[i]);
#ifndef NDEBUG
                    if (not db_station_lookup.contains(station_id)) {
                        db_station_lookup[station_id] = decoded_station_names[i];
                    }
#endif
                    segments.emplace_back(station_id, arrival_time, departure_time,
                                          i < station_num - 1 ? decoded_prices[i] : 0 // Last station has no price
                    );
                }
                // register into the train manager
                const auto decoded_sale_date = sale_date.as_vector();
                assert(decoded_sale_date.size() == 2);
                train_manager.add_train_group(train_group_name, segments, seat_num, decoded_sale_date[0],
                                              decoded_sale_date[1], type);
                global_interface::log.as(LogLevel::DEBUG)
                    << "Train group " << train_group_name << " has been added into the train manager" << '\n';
                // register into the ticket manager
                ticket_manager.add_train_group(
                    TrainManager::train_group_id_from_name(train_group_name), decoded_prices,
                    norb::Range<norb::Datetime::Date>(decoded_sale_date[0], decoded_sale_date[1]), seat_num);
                global_interface::log.as(LogLevel::DEBUG)
                    << "Train group " << train_group_name << " has been added into the ticket manager" << '\n';
                return 0;
            } catch (std::runtime_error &e) {
                global_interface::log.as(LogLevel::WARNING) << "Add train failed: " << e.what() << '\n';
                return -1;
            }
        }

        static int delete_train(const std::string &train_group_name) {
            auto &train_manager = get_instance().train_manager_;
            auto &ticket_manager = get_instance().ticket_manager_;

            try {
                const auto train_group_id = TrainManager::train_group_id_from_name(train_group_name);
                global_interface::log.as(LogLevel::DEBUG)
                    << "Deleting train group " << train_group_name << " with ID #" << train_group_id << '\n';
                train_manager.delete_train_group(train_group_id);
                ticket_manager.remove_train_group(train_group_id);
                return 0;
            } catch (std::runtime_error &e) {
                global_interface::log.as(LogLevel::WARNING) << "Delete train failed: " << e.what() << '\n';
                return -1;
            }
        }

        static int release_train(const std::string &train_group_name) {
            auto &train_manager = get_instance().train_manager_;
            auto &ticket_manager = get_instance().ticket_manager_;

            try {
                const auto train_group_id = TrainManager::train_group_id_from_name(train_group_name);
                global_interface::log.as(LogLevel::DEBUG)
                    << "Releasing train group " << train_group_name << " with ID #" << train_group_id << '\n';
                train_manager.release_train_group(train_group_id);
                ticket_manager.release_train_group(train_group_id);
                return 0;
            } catch (std::runtime_error &e) {
                global_interface::log.as(LogLevel::WARNING) << "Release train failed: " << e.what() << '\n';
                return -1;
            }
        }

        static void query_train_and_print(const std::string &train_group_name, const Date &date) {
            auto &train_manager = get_instance().train_manager_;
            auto &ticket_manager = get_instance().ticket_manager_;

            const auto train_group_id = TrainManager::train_group_id_from_name(train_group_name);
            const auto train_group_info = train_manager.get_train_group(train_group_id);

            if (not train_group_info.has_value()) {
                interface::log.as(LogLevel::WARNING)
                    << "Query train failed: train group " << train_group_name << " does not exist" << '\n';
                interface::out.as() << "-1\n";
                return;
            }
            if (not train_group_info->sale_date_range.contains(date)) {
                interface::log.as(LogLevel::WARNING) << "Query train failed: train group " << train_group_name
                                                     << " is not available on " << date << '\n';
                interface::out.as() << "-1\n";
                return;
            }
            interface::out.as() << train_group_info->train_group_name << ' ' << train_group_info->train_type << '\n';

            const auto train_id = train_id_t{train_group_id, date};
            const auto &train_group_segment_pointer = train_group_info->segment_pointer;
            int accumulated_price = 0;
            const auto has_released = train_manager.has_released_train_group(train_group_id);
            interface::log.as(LogLevel::INFO) << "Train group " << train_group_name << " has "
                                              << (has_released ? "been released" : "not been released") << '\n';
            norb::vector<int> seats_info;
            if (has_released) {
                seats_info = ticket_manager.get_remaining_seats_for_train(train_id);
                interface::log.as(LogLevel::DEBUG)
                    << "Queried remaining seats for train " << train_id << ": " << seats_info << '\n';
            }
            for (int i = 0; i < train_group_segment_pointer.size; ++i) {
                const auto &train_group_segment = train_manager.get_train_group_segment(train_group_segment_pointer, i);
                // const auto &ticket_segment = ticket_manager.get_train_status_station_segment(
                //     train_segment_pointer, std::min(i, train_group_segment_pointer.size - 2));

                // print on the same line
                interface::out.as(nullptr)
                    << train_manager.station_name_from_id(train_group_segment.station_id).value() << ' ';
                // arrival time
                if (i == 0) {
                    interface::out.as(nullptr) << datetime_placeholder << ' ';
                } else {
                    interface::out.as(nullptr) << (Datetime(date) + train_group_segment.arrival_time) << ' ';
                }
                interface::out.as(nullptr) << "-> ";
                // departure time
                if (i == train_group_segment_pointer.size - 1) {
                    interface::out.as(nullptr) << datetime_placeholder << ' ';
                } else {
                    interface::out.as(nullptr) << (Datetime(date) + train_group_segment.departure_time) << ' ';
                }
                // accumulated price + tickets left
                interface::out.as(nullptr) << accumulated_price << ' ';
                if (i < train_group_segment_pointer.size - 1) {
                    interface::out.as(nullptr) << (has_released ? seats_info[i] : train_group_info->seat_num) << '\n';
                    accumulated_price += train_group_segment.price;
                } else {
                    interface::out.as(nullptr) << 'x' << '\n';
                }
            }
        }

        struct TrainRideInfo {
            using price_t = TicketManager::price_t;
            using TrainRange = TrainManager::TrainRange;
            train_id_t train_id;
            station_id_t from_station_id;
            station_id_t to_station_id;
            Datetime from_time;
            Datetime to_time;
            price_t price;
            int remaining_seats;

            TrainRideInfo(const train_id_t &train_id, const station_id_t &from_station_id,
                          const station_id_t &to_station_id, const Datetime &from_time, const Datetime &to_time,
                          const price_t &price, const int &remaining_seats)
                : train_id(train_id), from_station_id(from_station_id), to_station_id(to_station_id),
                  from_time(from_time), to_time(to_time), price(price), remaining_seats(remaining_seats) {
            }

            TrainRideInfo(const TrainRange &tr, TrainManager &train_man, TicketManager &ticket_man) {
                train_id = tr.train_id;
                const auto train_group_info = train_man.get_train_group(tr.train_id.first);
                const auto from_train_group_seg =
                    train_man.get_train_group_segment(train_group_info->segment_pointer, tr.from_station_serial);
                const auto to_train_group_seg =
                    train_man.get_train_group_segment(train_group_info->segment_pointer, tr.to_station_serial);
                from_station_id = from_train_group_seg.station_id;
                to_station_id = to_train_group_seg.station_id;
                from_time = tr.from_time;
                to_time = tr.to_time;
                const auto train_fare_segment =
                    ticket_man.get_price_seat_for_section(tr.train_id, tr.from_station_serial, tr.to_station_serial);
                price = train_fare_segment.price;
                remaining_seats = train_fare_segment.remaining_seats;
            }
        };

        static norb::vector<TrainRideInfo> query_ticket(const station_id_t &from_id, const station_id_t &to_id,
                                                        const Date &date, const std::string &sort_by) {
            auto &ticket_manager = get_instance().ticket_manager_;
            auto &train_manager = get_instance().train_manager_;

            if (from_id == to_id) {
                interface::log.as(LogLevel::WARNING)
                    << "Query ticket failed: from and to stations are the same" << '\n';
                return {};
            }
            // find all trains that leave at from at date and arrive in to
            const auto &train_query_result = train_manager.query_ticket(from_id, to_id, Datetime(date), std::nullopt, true);
            // from ticket_manager retrieve the financial information
            norb::vector<TrainFareSegment> financial_info;
            norb::vector<std::string> train_names;
            norb::vector<Datetime> duration;
            for (const auto &train_item : train_query_result) {
                financial_info.push_back(ticket_manager.get_price_seat_for_section(
                    train_item.train_id, train_item.from_station_serial, train_item.to_station_serial));
                duration.push_back((train_item.to_time - train_item.from_time));
                train_names.push_back(train_manager.train_name_from_id(train_item.train_id.first).value());
            }
            norb::vector<int> sorted_indices;
            const int train_count = train_query_result.size();
            if (sort_by == "cost") {
                sorted_indices = norb::make_sorted(train_count, [&financial_info, &train_names](const int &a, const int &b) {
                    if (financial_info[a].price != financial_info[b].price)
                        return financial_info[a].price < financial_info[b].price;
                    return train_names[a] < train_names[b];
                });
            } else { // sort by time
                sorted_indices = norb::make_sorted(
                    train_count, [&duration, &train_names](const int &a, const int &b) {
                        if (duration[a] != duration[b])
                            return duration[a] < duration[b];
                        return train_names[a] < train_names[b];
                    });
            }
            norb::vector<TrainRideInfo> ret;
            // sort according to the sort_by parameter
            for (const auto &i : sorted_indices) {
                const auto &train_item = train_query_result[i];
                const auto &financial_item = financial_info[i];
                ret.emplace_back(train_item.train_id, from_id, to_id, train_item.from_time, train_item.to_time,
                                 financial_item.price, financial_item.remaining_seats);
            }
            return ret;
        }

        static void query_ticket_and_print(const std::string &from, const std::string &to, const Date &date,
                                           const std::string &sort_by) {
            auto &train_manager = get_instance().train_manager_;

            const auto from_id = TrainManager::station_id_from_name(from);
            const auto to_id = TrainManager::station_id_from_name(to);

            const auto query_ans = query_ticket(from_id, to_id, date, sort_by);
            interface::out.as() << query_ans.size() << '\n';
            for (const auto &item : query_ans) {
                const auto train_name = train_manager.get_train_group(item.train_id.first)->train_group_name;
                interface::out.as(nullptr)
                    << train_name << ' ' << from << ' ' << item.from_time << ' ' << "-> " << to << ' ' << item.to_time
                    << ' ' << item.price << ' ' << item.remaining_seats << '\n';
            }
        }

        static void query_transfer_and_print(const std::string &from, const std::string &to, const Date &date,
                                             const std::string &sort_by) {
            auto &train_manager = get_instance().train_manager_;

            const auto from_id = TrainManager::station_id_from_name(from);
            const auto to_id = TrainManager::station_id_from_name(to);
            if (from_id == to_id) {
                interface::log.as(LogLevel::WARNING)
                    << "Query transfer failed: from and to stations are the same" << '\n';
                interface::out.as() << "-1\n";
                return;
            }
            // iterate over the cities for mid and calculate the best trade between (from -> mid) and (mid -> to)
            using obj_t = std::pair<TrainRideInfo, TrainRideInfo>;
            norb::impl::cmp_func_t<obj_t> cmp_func;
            if (sort_by == "time") {
                cmp_func = [&train_manager](const obj_t &a, const obj_t &b) {
                    if (a.second.to_time - a.first.from_time != b.second.to_time - b.first.from_time) {
                        return a.second.to_time - a.first.from_time < b.second.to_time - b.first.from_time;
                    }
                    if (a.first.price + a.second.price != b.first.price + b.second.price) {
                        return a.first.price + a.second.price < b.first.price + b.second.price;
                    }
                    if (a.first.train_id != b.first.train_id) {
                        return train_manager.get_train_group(a.first.train_id.first)->train_group_name <
                               train_manager.get_train_group(b.first.train_id.first)->train_group_name;
                    } else {
                        return train_manager.get_train_group(a.second.train_id.first)->train_group_name <
                               train_manager.get_train_group(b.second.train_id.first)->train_group_name;
                    }
                };
            } else { // sort by cost
                cmp_func = [&train_manager](const obj_t &a, const obj_t &b) {
                    if (a.first.price + a.second.price != b.first.price + b.second.price) {
                        return a.first.price + a.second.price < b.first.price + b.second.price;
                    }
                    if (a.second.to_time - a.first.from_time != b.second.to_time - b.first.from_time) {
                        return a.second.to_time - a.first.from_time < b.second.to_time - b.first.from_time;
                    }
                    if (a.first.train_id != b.first.train_id) {
                        return train_manager.get_train_group(a.first.train_id.first)->train_group_name <
                               train_manager.get_train_group(b.first.train_id.first)->train_group_name;
                    } else {
                        return train_manager.get_train_group(a.second.train_id.first)->train_group_name <
                               train_manager.get_train_group(b.second.train_id.first)->train_group_name;
                    }
                };
            }
            norb::SupremeKeep<obj_t> best_transfer(cmp_func);
            for (const auto &mid_id : train_manager.station_id_vector) {
                // // todo remember to delete
                // const auto mid_station_name = train_manager.station_name_from_id(mid_id);
                // interface::log.as(LogLevel::DEBUG) << "Considering middle station: #" << mid_station_name.value() << '\n';
                const auto list_from_mid = query_ticket(from_id, mid_id, date, sort_by);
                interface::log.as(LogLevel::DEBUG) << "From to Mid containing " << list_from_mid.size() << " trains" << '\n';
                for (const auto &from_mid_train : list_from_mid) {
                    // // todo remember to delete
                    // const auto from_mid_train_group_info = train_manager.get_train_group(from_mid_train.train_id.first);

                    const auto datetime_at_arrival = from_mid_train.to_time;
                    const auto best_mid_to =
                        find_best_between(mid_id, to_id, datetime_at_arrival, sort_by, from_mid_train.train_id.first);
                    // // todo remember to delete
                    // const auto mid_to_train_group_info = train_manager.get_train_group(from_mid_train.train_id.first);

                    if (not best_mid_to.has_value()) {
                        continue; // skip if no valid train found
                    }
                    best_transfer.add(std::make_pair(from_mid_train, best_mid_to.value()));
                }
                // best_transfer.add(std::make_pair(best_from_mid.value(), best_mid_to.value()));
            }

            if (not best_transfer.val.has_value()) {
                interface::log.as(LogLevel::WARNING)
                    << "Query transfer failed: no valid transfer found from " << from << " to " << to << '\n';
                interface::out.as() << "0\n";
                return;
            } else {
                const auto [first_train, second_train] = best_transfer.val.value();
                const auto first_train_name =
                    train_manager.get_train_group(first_train.train_id.first)->train_group_name;
                const auto second_train_name =
                    train_manager.get_train_group(second_train.train_id.first)->train_group_name;
                const auto mid_station_name = train_manager.station_name_from_id(first_train.to_station_id).value();
                interface::out.as() << first_train_name << ' ' << from << ' ' << first_train.from_time << ' ' << "-> "
                                    << mid_station_name << ' ' << first_train.to_time << ' ' << first_train.price << ' '
                                    << first_train.remaining_seats << '\n';
                interface::out.as(nullptr) << second_train_name << ' ' << mid_station_name << ' ' << second_train.from_time
                                    << ' ' << "-> " << to << ' ' << second_train.to_time << ' ' << second_train.price
                                    << ' ' << second_train.remaining_seats << '\n';
                return;
            }
        }

        static std::variant<int, std::string> buy_ticket(const std::string &username,
                                                         const std::string &train_group_name, const Date &date,
                                                         const int &count, const std::string &from_station_name,
                                                         const std::string &to_station_name, const bool allow_queueing) {
            auto &ticket_manager = get_instance().ticket_manager_;
            auto &train_manager = get_instance().train_manager_;
            auto &account_manager = get_instance().account_manager_;

            const auto account_id = Account::id_from_username(username);
            if (not account_manager.is_active(account_id)) {
                global_interface::log.as(LogLevel::WARNING)
                    << "Buy ticket failed: user " << username << " is not logged in" << '\n';
                return -1;
            }
            const auto train_group_id = TrainManager::train_group_id_from_name(train_group_name);
            if (not train_manager.has_released_train_group(train_group_id)) {
                global_interface::log.as(LogLevel::WARNING) << "Buy ticket failed: train group " << train_group_name
                                                            << " does not exist or has not been released" << '\n';
                return -1;
            }
            const station_id_t from_station_id = TrainManager::station_id_from_name(from_station_name);
            const station_id_t to_station_id = TrainManager::station_id_from_name(to_station_name);
            const auto train_id = train_manager.deduce_train_id_from(train_group_id, date, from_station_id);
            if (not train_id.has_value()) {
                global_interface::log.as(LogLevel::WARNING)
                    << "Buy ticket failed: train group " << train_group_name << " does not have a train on " << date
                    << " from station " << from_station_name << '\n';
                return -1;
            }
            const auto train_group_info = train_manager.get_train_group(train_group_id);
            const auto from_station_serial =
                train_manager.get_station_serial_from_id(train_group_info.value(), from_station_id);
            const auto to_station_serial =
                train_manager.get_station_serial_from_id(train_group_info.value(), to_station_id);
            if (not to_station_serial.has_value()) {
                global_interface::log.as(LogLevel::WARNING)
                    << "Buy ticket failed: train group " << train_group_name << " does not have a train to station #"
                    << to_station_name << '\n';
                return -1;
            }
            if (train_group_info->seat_num < count) {
                global_interface::log.as(LogLevel::WARNING)
                    << "Buy ticket failed: requested more tickets than train group " << train_group_name << " has seats";
                return -1;
            }
            // Assert that the start station has a lower serial number than the destination
            if (from_station_serial.value() >= to_station_serial.value()) {
                global_interface::log.as(LogLevel::WARNING)
                    << "Buy ticket failed: destination comes earlier than departure station\n";
                return -1;
            }
            // Retrieve the price and remaining seats for the section
            const auto [price, remaining_seats] = ticket_manager.get_price_seat_for_section(
                train_id.value(), from_station_serial.value(), to_station_serial.value());
            interface::log.as(LogLevel::DEBUG) << "Train ID: " << train_id.value() << ", Price: " << price
                                               << ", Remaining Seats: " << remaining_seats << '\n';
            if (remaining_seats < count) {
                interface::log.as(LogLevel::INFO) << "Not enough seats available for purchase" << '\n';
                if (not allow_queueing) {
                    interface::log.as(LogLevel::WARNING) << "Buy ticket failed: Queueing is disabled" << '\n';
                    return -1; // Not enough seats and queueing is not allowed
                } else {
                    interface::log.as(LogLevel::INFO) << "Queueing is enabled, appending to queue" << '\n';
                    ticket_manager.register_order({.account = account_id,
                                                   .train_id = train_id.value(),
                                                   .from_station_serial = from_station_serial.value(),
                                                   .to_station_serial = to_station_serial.value(),
                                                   .from_time = train_manager.get_departure_datetime(train_group_info.value(), from_station_serial.value(), train_id->second),
                                                   .to_time = train_manager.get_arrival_datetime(train_group_info.value(), to_station_serial.value(), train_id->second),
                                                   .purchase_timestamp = interface::get_timestamp(),
                                                   .count = count,
                                                   .price = price,
                                                   .status = Order::Status::Pending});
                    return "queue";
                }
            } else {
                interface::log.as(LogLevel::DEBUG) << "Sufficient seats available, proceeding with purchase" << '\n';
                // Register the order
                ticket_manager.register_order({.account = account_id,
                                               .train_id = train_id.value(),
                                               .from_station_serial = from_station_serial.value(),
                                               .to_station_serial = to_station_serial.value(),
                                               .from_time = train_manager.get_departure_datetime(train_group_info.value(), from_station_serial.value(), train_id->second),
                                               .to_time = train_manager.get_arrival_datetime(train_group_info.value(), to_station_serial.value(), train_id->second),
                                               .purchase_timestamp = interface::get_timestamp(),
                                               .count = count,
                                               .price = price,
                                               .status = Order::Status::Success});
                return price * count; // Return the total price of the order
            }
        }

        static void query_order_and_print(const std::string &username) {
            auto &ticket_manager = get_instance().ticket_manager_;
            auto &account_manager = get_instance().account_manager_;
            auto &train_manager = get_instance().train_manager_;

            const auto account_id = Account::id_from_username(username);
            if (not account_manager.is_active(account_id)) {
                interface::log.as(LogLevel::WARNING)
                    << "Query order failed: user " << username << " is not logged in" << '\n';
                interface::out.as() << "-1\n";
                return;
            }
            const auto orders = ticket_manager.get_orders_by_account(account_id);
            if (orders.empty()) {
                interface::log.as(LogLevel::INFO) << "No orders found for user " << username << '\n';
                interface::out.as() << "0\n"; // No orders found
                return;
            }
            interface::out.as() << orders.size() << '\n';
            // [<STATUS>] <trainID> <FROM> <LEAVING_TIME> -> <TO> <ARRIVING_TIME> <PRICE> <NUM>
            // the newer an order is placed, the latter it comes in the vector
            // therefore, we should output them in reversed order
            for (int i = static_cast<int>(orders.size()) - 1; i >= 0; --i) {
                const auto &order = orders[i];
                const auto &train_group_info = train_manager.get_train_group(order.train_id.first);
                const auto &from_station_name = train_manager.station_name_from_id(
                    train_manager.get_train_group_segment(train_group_info->segment_pointer, order.from_station_serial).station_id
                );
                const auto &to_station_name = train_manager.station_name_from_id(
                    train_manager.get_train_group_segment(train_group_info->segment_pointer, order.to_station_serial).station_id
                );
                interface::out.as(nullptr)  << '[' << order.status_string() << "] "
                                            << train_group_info->train_group_name << ' '
                                            << from_station_name.value() << ' '
                                            << order.from_time << " -> "
                                            << to_station_name.value() << ' '
                                            << order.to_time << ' '
                                            << order.price << ' '
                                            << order.count << '\n';
                interface::log.as(LogLevel::DEBUG) << train_group_info->train_group_name << " registered at " << order.id() << '\n';
                interface::log.as(LogLevel::DEBUG) << "From station " << from_station_name.value() << " to " << to_station_name.value() << " at " << order.from_time << '\n';
                interface::log.as(LogLevel::DEBUG) << "Train ID: " << order.train_id << '\n';
                interface::log.as(LogLevel::DEBUG) << "From #" << order.from_station_serial << " to #" << order.to_station_serial << '\n';
            }
        }

        static int refund_ticket(const std::string &username, int order_serial) {
            --order_serial; // Convert to 0-based index
            auto &ticket_manager = get_instance().ticket_manager_;
            auto &account_manager = get_instance().account_manager_;

            const auto account_id = Account::id_from_username(username);
            // Verify that the user has logged in
            if (not account_manager.is_active(account_id)) {
                interface::log.as(LogLevel::WARNING)
                    << "Refund ticket failed: user " << username << " is not logged in" << '\n';
                return -1; // User is not logged in
            }
            // Retrieve the order
            const auto orders = ticket_manager.get_orders_by_account(account_id);
            if (order_serial < 0 or order_serial >= static_cast<int>(orders.size())) {
                interface::log.as(LogLevel::WARNING)
                    << "Refund ticket failed: order serial " << order_serial + 1 << " is out of range" << '\n';
                interface::log.as(LogLevel::DEBUG)
                    << "Total orders for user " << username << ": " << orders.size() << '\n';
                return -1; // Invalid order serial
            }
            // Modify the order_serial-th newest order
            const auto &order = orders[orders.size() - 1 - order_serial];
            try {
                ticket_manager.refund_order(order);
                return 0;
            }
            catch (const std::runtime_error &e) {
                interface::log.as(LogLevel::WARNING) << "Refund ticket failed: " << e.what() << '\n';
                return -1; // Refund failed
            }
        }

        static int clean() {
            auto &train_manager = get_instance().train_manager_;
            auto &ticket_manager = get_instance().ticket_manager_;
            auto &account_manager = get_instance().account_manager_;

            interface::log.as(LogLevel::INFO) << "Cleaning up the ticket system" << '\n';
            train_manager.clear();
            interface::log.as(LogLevel::DEBUG) << "Train manager cleaned" << '\n';
            ticket_manager.clear();
            interface::log.as(LogLevel::DEBUG) << "Ticket manager cleaned" << '\n';
            account_manager.clear();
            interface::log.as(LogLevel::DEBUG) << "Account manager cleaned" << '\n';

            return 0;
        }
    };
} // namespace ticket