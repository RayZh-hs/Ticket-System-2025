#pragma once

#include "account_manager.hpp"
#include "settings.hpp"
#include "ticket_manager.hpp"
#include "train_manager.hpp"
#include "utility/wrappers.hpp"

#include <variant>

namespace ticket {
    struct DatetimePlaceholder {};
    std::ostream &operator<<(std::ostream &os, const DatetimePlaceholder &) {
        os << "xx-xx xx:xx";
        return os;
    }
    inline constexpr DatetimePlaceholder datetime_placeholder{};

    class TicketSystem {
      private:
        AccountManager account_manager_;
        TrainManager train_manager_;
        TicketManager ticket_manager_;

        using LogLevel = norb::LogLevel;
        using account_id_t = Account::id_t;
        using Date = norb::Datetime::Date;
        using interface = global_interface;

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
                if (cur_user_account->privilege < privilege) {
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
            if (not(current_user_id == account_id or current_account_info->privilege >= account_info->privilege)) {
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
                // build the segments for the train manager
                std::vector<TrainGroupStationSegment> segments;
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
                    << "Train group " << train_group_name << " has been registered in the train manager" << '\n';
                // register into the ticket manager
                ticket_manager.add_train_group(
                    TrainManager::train_group_id_from_name(train_group_name), decoded_prices,
                    norb::Range<norb::Datetime::Date>(decoded_sale_date[0], decoded_sale_date[1]), seat_num);
                global_interface::log.as(LogLevel::DEBUG)
                    << "Train group " << train_group_name << " has been registered in the ticket manager" << '\n';
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

            try {
                const auto train_group_id = TrainManager::train_group_id_from_name(train_group_name);
                global_interface::log.as(LogLevel::DEBUG)
                    << "Releasing train group " << train_group_name << " with ID #" << train_group_id << '\n';
                train_manager.release_train_group(train_group_id);
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
            const auto train_status = ticket_manager.get_train_status(train_id);
            assert(train_status.has_value() && "Train status should exist if train group is available on the date");
            const auto &train_group_segment_pointer = train_group_info->segment_pointer;
            const auto &train_segment_pointer = train_status->segment_pointer;
            int accumulated_price = 0;
            for (int i = 0; i < train_group_segment_pointer.size; ++i) {
                const auto &train_group_segment = train_manager.get_train_group_segment(train_group_segment_pointer, i);
                const auto &ticket_segment = ticket_manager.get_train_status_station_segment(train_segment_pointer, std::min(i, train_group_segment_pointer.size - 2));

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
                    interface::out.as(nullptr) << ticket_segment.remaining_seats << '\n';
                    accumulated_price += train_group_segment.price;
                }
                else {
                    interface::out.as(nullptr) << 'x' << '\n';
                }
            }
        }
    };
} // namespace ticket