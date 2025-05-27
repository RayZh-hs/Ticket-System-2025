#pragma once

#include "account_manager.hpp"
#include "train_manager.hpp"

namespace ticket
{
    class TicketManager
    {
        using date_t = Train::date_t;
        using time_t = Train::time_t;
        using timespan_t = Train::timespan_t;

    public:
        void book_ticket(std::string &username, int ticket_num);
    };
}