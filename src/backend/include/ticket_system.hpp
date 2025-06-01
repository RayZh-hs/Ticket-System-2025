#pragma once

#include "account_manager.hpp"
#include "settings.hpp"
#include "ticket_manager.hpp"
#include "train_manager.hpp"

#include <variant>

namespace ticket {
    class TicketSystem {
      private:
        AccountManager account_manager_;
        TrainManager train_manager_;

        using LogLevel = norb::LogLevel;
        using account_id_t = Account::id_t;

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
            if (user_privilege >= current_user_privilege) {
                global_interface::log.as(LogLevel::WARNING)
                    << "QueryProfile failed because privilege underflow: " << current_user_privilege
                    << " <=" << user_privilege << '\n';
                return -1;
            }
            return account_info.value();
        }

        // static std::variant<int, Account> modify_profile(const std::string &current_username, const std::string &username, )
    };
} // namespace ticket