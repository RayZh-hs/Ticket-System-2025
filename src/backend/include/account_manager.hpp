#pragma once

#include "stlite/fixed_string.hpp"
#include "stlite/set.hpp"
#include "utils.hpp"

#include "settings.hpp"

#include <b_plus_tree.hpp>
#include <optional>

namespace ticket {
    struct Account {
        using username_t = norb::FixedString<max_username_length>;
        using hashed_password_t = global_hash_method::hash_t;
        using name_t = norb::FixedUTF8String<max_bytes_per_chinese_char * max_name_characters>;
        using mail_addr_t = norb::FixedString<max_mail_addr_length>;
        using privilege_t = int;

        username_t username;
        hashed_password_t hashed_password;
        name_t name;
        mail_addr_t mail_addr;
        privilege_t privilege = 0;

        // invoke type traits for BPT
        using id_t = global_hash_method::hash_t;
        [[nodiscard]] id_t id() const {
            return global_hash_method::hash(static_cast<std::string>(username));
        }
        static auto id_from_username(const std::string &username) {
            return global_hash_method::hash(username);
        }

        static auto hash_password(const std::string &password) {
            return global_hash_method::hash(password);
        }

        bool operator!=(const Account &other) const {
            return id() != other.id();
        }

        auto operator<=>(const Account &other) const {
            return id() <=> other.id();
        }

        friend std::ostream &operator<<(std::ostream &os, const Account &account);
    };

    inline std::ostream &operator<<(std::ostream &os, const Account &account) {
        os << account.username << ' ' << account.name << ' ' << account.mail_addr << ' ' << account.privilege;
        return os;
    }

    class AccountManager {
      public:
        using account_id_t = Account::id_t;
        using hashed_password_t = Account::hashed_password_t;

      private:
        using LogLevel = norb::LogLevel;
        norb::BPlusTree<Account::id_t, Account, norb::MANUAL> account_store;
        norb::set<Account::id_t> login_store;

      public:
        [[nodiscard]] size_t count_registered_users() const {
            return account_store.size();
        }

        [[nodiscard]] size_t count_login_users() const {
            return login_store.size();
        }

        [[nodiscard]] std::optional<Account> find_user(const account_id_t &account_id) const {
            const auto found = account_store.find_all(account_id);
            global_interface::log << "Find user for " << account_id << " yielded " << found << '\n';
            assert(found.size() <= 1);
            return (!found.empty()) ? std::optional(found[0]) : std::nullopt;
        }

        [[nodiscard]] std::optional<Account> find_active_user(const account_id_t &account_id) const {
            if (login_store.count(account_id)) {
                return find_user(account_id);
            }
            return std::nullopt;
        }

        [[nodiscard]] bool is_registered(const account_id_t &account_id) const {
            return !account_store.find_all(account_id).empty();
        }

        [[nodiscard]] bool is_active(const account_id_t &account_id) const {
            return login_store.count(account_id);
        }

        void add_user(const Account &account) {
            const auto account_id = account.id();
            if (is_registered(account_id)) {
                global_interface::log.as(LogLevel::WARNING)
                    << "Error while adding user: User #" + account_id << " already exists!";
                throw std::runtime_error("Existing user");
            }
            // register the new account
            account_store.insert(account_id, account);
        }

        void login(const account_id_t &account_id, const hashed_password_t &hashed_password) {
            if (is_active(account_id)) {
                global_interface::log.as(LogLevel::WARNING)
                    << "Error: User #" << account_id << " has already been logged in\n";
                throw std::runtime_error("Attempting to login active user");
            }
            const auto account_info = find_user(account_id);
            if (not account_info.has_value()) {
                global_interface::log.as(LogLevel::WARNING) << "Error: User #" << account_id << " does not exist\n";
                throw std::runtime_error("Attempting to login non-existent user");
            }
            if (account_info.value().hashed_password != hashed_password) {
            }
            login_store.insert(account_id);
        }

        void logout(const account_id_t &account_id) {
            if (is_active(account_id)) {
                login_store.erase(login_store.find(account_id));
            } else {
                global_interface::log.as(LogLevel::WARNING) << "Error: User #" << account_id << " has not logged in\n";
                throw std::runtime_error("Attempting to logout non-active user");
            }
        }

        void change_account_info(const account_id_t &account_id, const Account &account) {
            account_store.remove_all(account_id);
            account_store.insert(account_id, account);
        }

        void clear() {
            account_store.clear();
            login_store.clear();
        }
    };
} // namespace ticket
