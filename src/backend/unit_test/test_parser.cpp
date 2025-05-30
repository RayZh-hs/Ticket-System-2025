#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <functional>

#include "utility/parser.hpp"

constexpr auto is_flag = ticket::CommandRegistry::is_flag;

int tests_run = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition) \
    do { \
        tests_run++; \
        if (!(condition)) { \
            tests_failed++; \
            std::cerr << "TEST FAILED: " << __FILE__ << ":" << __LINE__ \
                      << " Condition: " #condition << std::endl; \
        } \
    } while (0)

#define TEST_THROWS(expression, ExceptionType) \
    do { \
        tests_run++; \
        bool thrown = false; \
        std::string exception_what; \
        try { \
            (expression); \
        } catch (const ExceptionType& e) { \
            thrown = true; \
            exception_what = e.what(); \
            std::cout << "  Expected exception caught: " << exception_what << std::endl; \
        } catch (const std::exception& e) { \
            tests_failed++; \
            std::cerr << "TEST FAILED: " << __FILE__ << ":" << __LINE__ \
                      << " Expression: " #expression " threw std::exception: " << e.what() \
                      << " (expected " #ExceptionType ")" << std::endl; \
        } catch (...) { \
            tests_failed++; \
            std::cerr << "TEST FAILED: " << __FILE__ << ":" << __LINE__ \
                      << " Expression: " #expression " threw unknown exception type." << std::endl; \
        } \
        if (!thrown) { \
            tests_failed++; \
            std::cerr << "TEST FAILED: " << __FILE__ << ":" << __LINE__ \
                      << " Expression: " #expression " did not throw " #ExceptionType << std::endl; \
        } \
    } while (0)

static int g_s_flag = -1, g_t_flag = -1, g_p_value = -1;
static std::string g_d_value = "UNSET";
void mock_query_ticket_cmd(int s_flag, int t_flag, const std::string& d_value, int p_value) {
    g_s_flag = s_flag; g_t_flag = t_flag; g_d_value = d_value; g_p_value = p_value;
    std::cout << "  mock_query_ticket_cmd called: s=" << s_flag << ", t=" << t_flag
              << ", d='" << d_value << "', p=" << p_value << std::endl;
}

static std::string g_username = "UNSET";
static int g_id_val = -1;
static bool g_active_flag = false;
void mock_user_profile_cmd(const std::string& username, int id_val, bool active_flag) {
    g_username = username; g_id_val = id_val; g_active_flag = active_flag;
    std::cout << "  mock_user_profile_cmd called: u='" << username << "', i=" << id_val
              << ", a=" << (active_flag ? "true" : "false") << std::endl;
}

static bool g_no_args_called = false;
void mock_no_args_cmd() {
    g_no_args_called = true;
    std::cout << "  mock_no_args_cmd called." << std::endl;
}

static std::string g_setting = "UNSET";
static int g_level = -1;
static bool g_verbose_flag = false;
void mock_config_cmd(const std::string& setting, int level, bool verbose_flag) {
    g_setting = setting; g_level = level; g_verbose_flag = verbose_flag;
    std::cout << "  mock_config_cmd called: s='" << setting << "', l=" << level
              << ", v=" << (verbose_flag ? "true" : "false") << std::endl;
}

// For test_registry_dispatch_errors
static std::string g_needs_arg_d_value = "UNSET_NEEDS_ARG";
void mock_needs_arg_cmd(const std::string& d_param) {
    g_needs_arg_d_value = d_param;
    std::cout << "  mock_needs_arg_cmd called: d='" << d_param << "'" << std::endl;
}
static int g_needs_int_p_value = -1;
void mock_needs_int_cmd(int p_val){
    g_needs_int_p_value = p_val;
    std::cout << "  mock_needs_int_cmd called: p=" << p_val << std::endl;
};


void reset_mock_globals() {
    g_s_flag = -1; g_t_flag = -1; g_d_value = "UNSET"; g_p_value = -1;
    g_username = "UNSET"; g_id_val = -1; g_active_flag = false;
    g_no_args_called = false;
    g_setting = "UNSET"; g_level = -1; g_verbose_flag = false;
    g_needs_arg_d_value = "UNSET_NEEDS_ARG";
    g_needs_int_p_value = -1;
}


void test_parser_valid_inputs() {
    std::cout << "\n--- Testing Parser Valid Inputs ---" << std::endl;
    ticket::Instruction inst;

    reset_mock_globals();
    inst = ticket::Parser::parse("[100] my_cmd -a -b val_b -c");
    TEST_ASSERT(inst.timestamp == 100);
    TEST_ASSERT(inst.command == "my_cmd");
    TEST_ASSERT(inst.kwargs.size() == 3);
    if (inst.kwargs.size() == 3) {
        TEST_ASSERT(inst.kwargs[0].first == 'a' && inst.kwargs[0].second == "");
        TEST_ASSERT(inst.kwargs[1].first == 'b' && inst.kwargs[1].second == "val_b");
        TEST_ASSERT(inst.kwargs[2].first == 'c' && inst.kwargs[2].second == "");
    }

    reset_mock_globals();
    inst = ticket::Parser::parse("[0] another_cmd");
    TEST_ASSERT(inst.timestamp == 0);
    TEST_ASSERT(inst.command == "another_cmd");
    TEST_ASSERT(inst.kwargs.empty());

    reset_mock_globals();
    inst = ticket::Parser::parse("  [123]   spaced_cmd   -x   val_for_x  ");
    TEST_ASSERT(inst.timestamp == 123);
    TEST_ASSERT(inst.command == "spaced_cmd");
    TEST_ASSERT(inst.kwargs.size() == 1);
    if (inst.kwargs.size() == 1) {
        TEST_ASSERT(inst.kwargs[0].first == 'x' && inst.kwargs[0].second == "val_for_x");
    }
}

void test_parser_invalid_inputs() {
    std::cout << "\n--- Testing Parser Invalid Inputs ---" << std::endl;
    TEST_THROWS(ticket::Parser::parse(""), std::runtime_error);
    TEST_THROWS(ticket::Parser::parse("not_an_int my_cmd"), std::runtime_error);
    TEST_THROWS(ticket::Parser::parse("100"), std::runtime_error);
    TEST_THROWS(ticket::Parser::parse("100 my_cmd positional_arg"), std::runtime_error);
    TEST_THROWS(ticket::Parser::parse("100 my_cmd -"), std::runtime_error);
}

void test_param_info_api() {
    std::cout << "\n--- Testing ParamInfo API Variations ---" << std::endl;
    ticket::CommandRegistry::ParamInfo p1('a');
    TEST_ASSERT(p1.key == 'a' && !p1.is_flag && !p1.default_value_str.has_value());

    ticket::CommandRegistry::ParamInfo p2('b', is_flag);
    TEST_ASSERT(p2.key == 'b' && p2.is_flag && !p2.default_value_str.has_value());

    ticket::CommandRegistry::ParamInfo p3('c', "default_val");
    TEST_ASSERT(p3.key == 'c' && !p3.is_flag && p3.default_value_str.has_value() && *p3.default_value_str == "default_val");

    ticket::CommandRegistry::ParamInfo p4('d', 123);
    TEST_ASSERT(p4.key == 'd' && !p4.is_flag && p4.default_value_str.has_value() && *p4.default_value_str == "123");

    ticket::CommandRegistry::ParamInfo p5('e', true);
    TEST_ASSERT(p5.key == 'e' && !p5.is_flag && p5.default_value_str.has_value() && *p5.default_value_str == "true");
}

void test_registry_dispatch_valid() {
    std::cout << "\n--- Testing Registry Dispatch Valid Cases ---" << std::endl;
    ticket::CommandRegistry registry;

    registry.register_command("query_ticket", mock_query_ticket_cmd, {
        {'s', is_flag}, {'t', is_flag}, {'d'}, {'p', 0}
    });
    registry.register_command("user_profile", mock_user_profile_cmd, {
        {'u'}, {'i'}, {'a', is_flag}
    });
    registry.register_command("no_args", mock_no_args_cmd, {});
    registry.register_command("config", mock_config_cmd, {
        {'s', "default_s"}, {'l', 99}, {'v', is_flag}
    });

    ticket::Instruction inst;

    reset_mock_globals();
    inst = ticket::Parser::parse("[101] query_ticket -s -d TestD -p 123");
    registry.dispatch(inst);
    TEST_ASSERT(g_s_flag == 1);
    TEST_ASSERT(g_t_flag == 0);
    TEST_ASSERT(g_d_value == "TestD");
    TEST_ASSERT(g_p_value == 123);

    reset_mock_globals();
    inst = ticket::Parser::parse("[102] query_ticket -d AnotherD");
    registry.dispatch(inst);
    TEST_ASSERT(g_s_flag == 0);
    TEST_ASSERT(g_t_flag == 0);
    TEST_ASSERT(g_d_value == "AnotherD");
    TEST_ASSERT(g_p_value == 0);

    reset_mock_globals();
    inst = ticket::Parser::parse("[201] user_profile -u TestUser -i 789 -a");
    registry.dispatch(inst);
    TEST_ASSERT(g_username == "TestUser");
    TEST_ASSERT(g_id_val == 789);
    TEST_ASSERT(g_active_flag == true);

    reset_mock_globals();
    inst = ticket::Parser::parse("[202] user_profile -u OtherUser -i 1011");
    registry.dispatch(inst);
    TEST_ASSERT(g_username == "OtherUser");
    TEST_ASSERT(g_id_val == 1011);
    TEST_ASSERT(g_active_flag == false);

    reset_mock_globals();
    inst = ticket::Parser::parse("[301] no_args");
    registry.dispatch(inst);
    TEST_ASSERT(g_no_args_called == true);

    reset_mock_globals();
    inst = ticket::Parser::parse("[401] config");
    registry.dispatch(inst);
    TEST_ASSERT(g_setting == "default_s");
    TEST_ASSERT(g_level == 99);
    TEST_ASSERT(g_verbose_flag == false);

    reset_mock_globals();
    inst = ticket::Parser::parse("[402] config -s CustomS -v");
    registry.dispatch(inst);
    TEST_ASSERT(g_setting == "CustomS");
    TEST_ASSERT(g_level == 99);
    TEST_ASSERT(g_verbose_flag == true);
}

void test_registry_dispatch_errors() {
    std::cout << "\n--- Testing Registry Dispatch Errors ---" << std::endl;
    ticket::CommandRegistry registry;

    registry.register_command("needs_arg", mock_needs_arg_cmd, { {'d'} });
    registry.register_command("needs_int", mock_needs_int_cmd, { {'p'} });


    ticket::Instruction inst;

    reset_mock_globals();
    inst = ticket::Parser::parse("[999] unknown_cmd -x");
    TEST_THROWS(registry.dispatch(inst), std::runtime_error);

    reset_mock_globals();
    inst = ticket::Parser::parse("[100] needs_arg"); // -d is missing
    TEST_THROWS(registry.dispatch(inst), std::runtime_error);

    reset_mock_globals();
    inst = ticket::Parser::parse("[101] needs_arg -d ValueForD"); // Successful call
    registry.dispatch(inst);
    TEST_ASSERT(g_needs_arg_d_value == "ValueForD");

    reset_mock_globals();
    inst = ticket::Parser::parse("[200] needs_int -p not_an_int");
    TEST_THROWS(registry.dispatch(inst), std::runtime_error);

    reset_mock_globals();
    inst = ticket::Parser::parse("[201] needs_int -p 12345"); // Successful call
    registry.dispatch(inst);
    TEST_ASSERT(g_needs_int_p_value == 12345);
}


int main() {
    std::cout << "Starting Unit Tests for utility/parser.hpp" << std::endl;

    test_parser_valid_inputs();
    test_parser_invalid_inputs();
    test_param_info_api();
    test_registry_dispatch_valid();
    test_registry_dispatch_errors();

    std::cout << "\n--- Test Summary ---" << std::endl;
    std::cout << "Tests Run: " << tests_run << std::endl;
    std::cout << "Tests Failed: " << tests_failed << std::endl;

    if (tests_failed == 0) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << tests_failed << " tests FAILED!" << std::endl;
        return 1;
    }
}