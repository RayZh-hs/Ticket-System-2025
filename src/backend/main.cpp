#include "ticket_system.hpp"
#include "utility/decorators.hpp"
#include "utility/parser.hpp"

using interface = ticket::global_interface;
using norb::$print;
using norb::LogLevel;
using ticket::command_registry_error;
using ticket::CommandRegistry;
using ticket::parser_error;
using ticket::TicketSystem;

void register_commands(CommandRegistry &);

int main() {
    norb::chore::remove_associated();
    // std::freopen("../testcases/1867/2.in", "r", stdin);
    CommandRegistry cmdr;
    register_commands(cmdr);
    std::string line;
    while (std::getline(interface::in, line)) {
        // Bypass empty lines
        if (line.empty())
            continue;
        // Decode the instruction
        const auto instruction = ticket::Parser::parse(line);
        // Set global timestamp
        interface::set_timestamp(instruction.timestamp);
        // Special impl for exit
        if (instruction.command == "exit") {
            interface::out.as() << "bye" << '\n';
            break;
        }
        try {
            cmdr.dispatch(instruction);
        } catch (command_registry_error &e) {
            // Illegal instruction
            interface::log.as(LogLevel::ERROR) << "Command registry error: " << e.what() << '\n';
            interface::out.as() << -1 << '\n';
        }
    }
    interface::log.as(LogLevel::INFO) << "Exiting ticket system\n";
    return 0;
}

void register_commands(CommandRegistry &cmdr) {
    cmdr.register_command("add_user", $print(TicketSystem::add_user),
                          {
                              {'c', ""}, // current_user, "" for first call
                              {'u'},     // username
                              {'p'},     // password
                              {'n'},     // name
                              {'m'},     // mail address
                              {'g', ""}  // privilege
                          });
    cmdr.register_command("login", $print(TicketSystem::login),
                          {
                              {'u'}, // username
                              {'p'}  // password
                          });
    cmdr.register_command("logout", $print(TicketSystem::logout),
                          {
                              {'u'} // username
                          });
    cmdr.register_command("query_profile", $print(TicketSystem::query_profile),
                          {
                              {'c'}, // current user
                              {'u'}  // username
                          });
    cmdr.register_command("modify_profile", $print(TicketSystem::modify_profile),
                          {
                              {'c'},               // current user
                              {'u'},               // username
                              {'p', std::nullopt}, // password
                              {'n', std::nullopt}, // name
                              {'m', std::nullopt}, // mail address
                              {'g', std::nullopt}  // privilege
                          });
    cmdr.register_command("add_train", $print(TicketSystem::add_train),
                          {
                              {'i'}, // train group name (aka. trainID)
                              {'n'}, // station num
                              {'m'}, // seat num
                              {'s'}, // station names
                              {'p'}, // prices
                              {'x'}, // start time
                              {'t'}, // travel times
                              {'o'}, // stopover times
                              {'d'}, // sale date
                              {'y'}  // type
                          });
    cmdr.register_command("delete_train", $print(TicketSystem::delete_train),
                          {
                              {'i'}, // train group name (aka. trainID)
                          });
    cmdr.register_command("release_train", $print(TicketSystem::release_train),
                          {
                              {'i'}, // train group name (aka. trainID)
                          });
    cmdr.register_command("query_train", TicketSystem::query_train_and_print,
                          {
                              {'i'}, // train group name (aka. trainID)
                              {'d'}  // date of departure
                          });
    cmdr.register_command("query_ticket", TicketSystem::query_ticket_and_print,
                          {
                              {'s'},        // from station name
                              {'t'},        // to station name
                              {'d'},        // date of departure
                              {'p', "time"} // sort by "time" or "price"
                          });
    cmdr.register_command("query_transfer", TicketSystem::query_transfer_and_print,
                          {
                              {'s'},        // from station name
                              {'t'},        // to station name
                              {'d'},        // date of departure
                              {'p', "time"} // sort by "time" or "price"
                          });
    cmdr.register_command("buy_ticket", $print(TicketSystem::buy_ticket),
                          {
                              {'u'},       // username
                              {'i'},       // train group name (aka. trainID)
                              {'d'},       // date of departure
                              {'n'},       // the number of tickets to buy
                              {'f'},       // from station name
                              {'t'},       // to station name
                              {'q', false} // allow queueing, default is false
                          });
    cmdr.register_command("query_order", TicketSystem::query_order_and_print,
                          {
                              {'u'} // username
                          });
    cmdr.register_command("refund_ticket", $print(TicketSystem::refund_ticket),
                          {
                              {'u'},   // username
                              {'n', 1} // order id
                          });
    cmdr.register_command("clean", $print(TicketSystem::clean), {});
}