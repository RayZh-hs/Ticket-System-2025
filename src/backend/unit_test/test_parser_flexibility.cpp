#include "utility/parser.hpp"
#include "utility/wrappers.hpp"
#include <iostream>

constexpr auto is_flag = ticket::CommandRegistry::is_flag;

void foo(const ticket::ConcentratedString<int> &str) {
    const auto vect = str.as_vector();
    std::cout << vect << '\n';
}

int main() {
    ticket::CommandRegistry cmdr;
    cmdr.register_command("print", foo, {{'s'}});

    cmdr.dispatch(ticket::Parser::parse("[1] print -s 10|25|35"));
}