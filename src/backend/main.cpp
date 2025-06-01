#include "ticket_system.hpp"
#include "utility/decorators.hpp"
#include "utility/parser.hpp"

using interface = ticket::global_interface;
using norb::LogLevel;
using ticket::command_registry_error;
using ticket::CommandRegistry;
using ticket::parser_error;
using ticket::TicketSystem;
using norb::$print;

void register_commands(CommandRegistry &);

int main() {
    norb::chore::remove_associated();
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
        if (instruction.command == "exit")
            break;
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
}