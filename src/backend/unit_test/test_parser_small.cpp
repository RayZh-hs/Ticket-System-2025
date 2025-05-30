#include <iostream>
#include "utility/parser.hpp"

constexpr auto is_flag = ticket::CommandRegistry::is_flag;

int a = 0, b = 0;

void incr(const bool &for_a, const bool &for_b, const long &amount)
{
    if (for_a)  std::cout << "Increment a: " << (a += amount) << '\t';
    if (for_b)  std::cout << "Increment b: " << (b += amount) << '\t';
    std::cout << '\n';
}

void print()
{
    std::cout << a << ' ' << b << '\n';
}

int main()
{
    ticket::CommandRegistry cmdr;
    cmdr.register_command("incr", incr, {
        {'a', is_flag},
        {'b', is_flag},
        {'x'}
    });
    cmdr.register_command("print", print, {});

    cmdr.dispatch(ticket::Parser::parse("[0] print"));
    cmdr.dispatch(ticket::Parser::parse("[1] incr -b -a -x 5"));
    cmdr.dispatch(ticket::Parser::parse("[2] incr -a -x 1"));
    cmdr.dispatch(ticket::Parser::parse("[3] print"));
}