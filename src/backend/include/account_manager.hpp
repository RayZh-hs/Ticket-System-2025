#pragma once

#include "stlite/fixed_string.hpp"
#include "stlite/map.hpp"
#include "utils.hpp"

#include "settings.hpp"

#include <b_plus_tree.hpp>
#include <optional>

namespace ticket {
    struct Account {
        using username_t = norb::FixedString<max_username_length>;
        using password_t = norb::FixedString<max_password_length>;
        using name_t = norb::FixedUTF8String<max_bytes_per_chinese_char * max_name_characters>;
        using mail_addr_t = norb::FixedString<max_mail_addr_length>;
        using privilege_t = int;

        username_t username;
        password_t password;
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

        auto operator <=> (const Account &other) const {
            return id() <=> other.id();
        }
    };

    class AccountManager {
      public:
        using account_id_t = Account::id_t;

      private:
        norb::BPlusTree<Account::id_t, Account> account_store;
        norb::map<Account::id_t, Account> login_store;

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

        [[nodiscard]] bool is_registered(const account_id_t &account_id) const {
            return !account_store.find_all(account_id).empty();
        }

        [[nodiscard]] std::optional<Account> find_active_user(const account_id_t &account_id) const {
            if (const auto found = login_store.find(account_id); found == login_store.cend())
                return std::nullopt;
            else
                return found->second;
        }

        [[nodiscard]] bool is_active(const account_id_t &account_id) const {
            return login_store.count(account_id);
        }

        void add_user(const Account &account) {
            const auto account_id = account.id();
            if (is_registered(account_id)) {
                global_interface::log << "Error while adding user: User #" + account_id << " already exists!\n";
                throw std::runtime_error("Existing user");
            }
            // register the new account
            account_store.insert(account_id, account);
        }

        // void
    };
} // namespace ticket
