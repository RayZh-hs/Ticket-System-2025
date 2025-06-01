#include "ticket_system.hpp"
#include "utility/decorators.hpp"
#include "utility/parser.hpp"

// Using common
using interface = ticket::global_interface;
using ticket::CommandRegistry;
using ticket::TicketSystem;
// Using decorators:
using norb::$print;

void register_commands(CommandRegistry &);

int main() {
    norb::chore::remove_associated();
    CommandRegistry cmdr;
    register_commands(cmdr);
    std::string line;
    while (std::getline(interface::in, line)) {
        const auto instruction = ticket::Parser::parse(line);
        norb::logger::set_line_number(instruction.timestamp);
        cmdr.dispatch(instruction);
    }
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
}