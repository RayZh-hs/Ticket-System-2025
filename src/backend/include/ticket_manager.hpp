#pragma once

#include "account_manager.hpp"
#include "settings.hpp"

namespace ticket {
    struct PurchaseHistory {};

    class TicketManager {
      private:
        using LogLevel = norb::LogLevel;
        using account_id_t = Account::id_t;

        norb::BPlusTree<account_id_t, PurchaseHistory> purchase_history_store;
    };
} // namespace ticket