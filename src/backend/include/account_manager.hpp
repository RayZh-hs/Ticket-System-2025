#pragma once

#include <utils.hpp>
#include <stlite/fixed_string.hpp>

#include "settings.hpp"

namespace ticket
{
    struct Account
    {
        using username_t = norb::FixedString<max_username_length>;
        using password_t = norb::FixedString<max_password_length>;
        using name_t = norb::FixedUTF8String<max_bytes_per_chinese_char * max_name_characters>;
        using mail_addr_t = norb::FixedString<max_mail_addr_length>;
        using privilege_t = int;

        username_t username;
        password_t password;
        name_t name;
        mail_addr_t mail_addr;
        privilege_t privilege;
    };

    class AccountManager
    {
        using hash_method = norb::hash::Djb2Hash;
        using hash_t = hash_method::hash_t;

    public:
    };
}
