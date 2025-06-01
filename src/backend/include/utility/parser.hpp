#pragma once

#include <functional>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "semantic_cast.hpp"
#include "stlite/map.hpp"
#include "stlite/vector.hpp"
#include "utility/decorators.hpp"
#include "utils.hpp"

namespace ticket {

    struct parser_error final : std::logic_error {
        parser_error(const std::string &msg) : std::logic_error(msg) {
        }
    };
    struct command_registry_error final : std::logic_error {
        command_registry_error(const std::string &msg) : std::logic_error(msg) {
        }
    };

    struct Instruction {
        using timestamp_t = int;
        using command_t = std::string;
        using key_t = char;
        using arg_t = std::string;

        timestamp_t timestamp = 0;
        command_t command;
        std::vector<std::pair<key_t, arg_t>> kwargs;
    };

    class Parser {
      public:
        // In ticket::Parser class
        static Instruction parse(const std::string &str_in) {
            Instruction inst;
            const std::string s = norb::string::trim(str_in);

            if (s.empty()) { // Check again after all trimming
                throw parser_error("Parser::parse error: Input string is empty after trimming.");
            }

            std::istringstream iss(s);
            std::string token;

            if (!(iss >> token))
                throw parser_error("Parser::parse error: Missing timestamp.");
            try {
                // token is formed as [INTEGER]
                assert(token.size() >= 3);
                inst.timestamp = std::stoi(token.substr(1, token.size() - 2));
            } catch (const std::exception &e) {
                throw parser_error("Parser::parse error: Invalid timestamp '" + token + "'. " + e.what());
            }

            if (!(iss >> inst.command) || inst.command.empty()) {
                throw parser_error("Parser::parse error: Missing or empty command name after timestamp.");
            }

            while (iss >> token) {
                if (token.length() >= 2 && token[0] == '-') {
                    char key_char = token[1];
                    std::string value_arg;

                    const std::streampos current_pos_before_value_peek = iss.tellg();
                    std::string next_token_peek;

                    if (iss >> next_token_peek) {
                        if (!next_token_peek.empty() && next_token_peek[0] != '-') {
                            value_arg = next_token_peek;
                        } else {
                            iss.seekg(current_pos_before_value_peek);
                        }
                    } else {
                        iss.clear();
                    }
                    inst.kwargs.emplace_back(key_char, value_arg);
                } else {
                    throw parser_error("Parser::parse error: Unexpected token '" + token +
                                       "' in arguments. Expected a key (e.g., -k).");
                }
            }
            return inst;
        }
    };

    namespace detail_traits {
        // Primary template - left undefined. Instantiation means no specialization matched.
        template <typename T> struct function_traits_impl;

        // Specialization for function pointers
        template <typename Ret, typename... Args> struct function_traits_impl<Ret (*)(Args...)> {
            using return_type = Ret;
            using arg_tuple_type = std::tuple<Args...>;
            static constexpr size_t arity = sizeof...(Args);
        };

        // Specialization for std::function
        template <typename Ret, typename... Args> struct function_traits_impl<std::function<Ret(Args...)>> {
            using return_type = Ret;
            using arg_tuple_type = std::tuple<Args...>;
            static constexpr size_t arity = sizeof...(Args);
        };

        // Specialization for norb::PrintResult (THIS IS KEY)
        // It "unwraps" the decorator to get traits of the original function.
        // This should be chosen by the compiler over the general class type rule below for PrintResult instances.
        template <typename WrappedCallable, typename OstreamLike>
        struct function_traits_impl<norb::PrintResult<WrappedCallable, OstreamLike>>
            : function_traits_impl<std::decay_t<WrappedCallable>> {}; // Recurse on the wrapped type

        // Specializations for member function pointers (operator() of functors/lambdas)
        // These are what decltype(&T::operator()) would resolve to.
        template <typename ClassType, typename Ret, typename... Args>
        struct function_traits_impl<Ret (ClassType::*)(Args...) const> {
            using return_type = Ret;
            using arg_tuple_type = std::tuple<Args...>;
            static constexpr size_t arity = sizeof...(Args);
        };
        template <typename ClassType, typename Ret, typename... Args>
        struct function_traits_impl<Ret (ClassType::*)(Args...)> { // Non-const
            using return_type = Ret;
            using arg_tuple_type = std::tuple<Args...>;
            static constexpr size_t arity = sizeof...(Args);
        };

        // General fallback for other class types (functors/lambdas)
        // This will be used if T is a class and not PrintResult (due to PrintResult being more specific)
        // and not one of the function pointer/std::function types.
        template <typename T> struct function_traits_impl : function_traits_impl<decltype(&T::operator())> {};
        // This rule is fine for lambdas. It would have been problematic for PrintResult
        // if the PrintResult-specific specialization wasn't present or wasn't chosen.

    } // namespace detail_traits
    template <typename T> using function_traits = detail_traits::function_traits_impl<std::decay_t<T>>;

    class CommandRegistry {
      public:
        struct FlagIndicator {};
        static constexpr FlagIndicator is_flag{};

        struct ParamInfo {
            char key;
            bool is_flag;
            std::optional<std::string> default_value_str;

            // Constructor for: {'k'} (required, no default)
            ParamInfo(char k) : key(k), is_flag(false), default_value_str(std::nullopt) {
            }

            // Constructor for: {'k', flag}
            ParamInfo(char k, FlagIndicator) : key(k), is_flag(true), default_value_str(std::nullopt) {
            }

            // Constructor for: {'k', "default_string"}
            ParamInfo(char k, const char *default_val)
                : key(k), is_flag(false), default_value_str(std::string(default_val)) {
            }

            ParamInfo(char k, const std::string &default_val) : key(k), is_flag(false), default_value_str(default_val) {
            }

            // Constructor for: {'k', int_default_val} e.g. {'p', 1}
            ParamInfo(char k, int default_val)
                : key(k), is_flag(false), default_value_str(std::to_string(default_val)) {
            }

            // Constructor for: {'k', bool_default_val} e.g. {'b', true}
            ParamInfo(char k, bool default_val)
                : key(k), is_flag(false), default_value_str(default_val ? "true" : "false") {
            }

            // Add more overloads for other default types if needed (double, etc.)
        };

      private:
        norb::map<std::string, std::function<void(const Instruction &)>> handlers_;

        template <typename TargetTypeOriginal>
        std::decay_t<TargetTypeOriginal> get_value_for_param(const Instruction &inst,
                                                             const ParamInfo &param_info) const {
            using DecayedTargetType = std::decay_t<TargetTypeOriginal>;
            bool key_found = false;
            std::string kwarg_val_str; // Value from instruction's kwargs if key is found

            for (const auto &kwarg : inst.kwargs) {
                if (kwarg.first == param_info.key) {
                    key_found = true;
                    kwarg_val_str = kwarg.second;
                    break;
                }
            }

            if (key_found) {
                if (param_info.is_flag) {
                    if constexpr (std::is_same_v<DecayedTargetType, bool>)
                        return true;
                    if constexpr (std::is_same_v<DecayedTargetType, int>)
                        return 1;
                }
                return norb::semantic_cast<DecayedTargetType>(kwarg_val_str);
            }

            // Key not found
            if (param_info.is_flag) {
                if constexpr (std::is_same_v<DecayedTargetType, bool>)
                    return false;
                if constexpr (std::is_same_v<DecayedTargetType, int>)
                    return 0;
                // A flag for a non-bool/int type that is missing
                throw command_registry_error(
                    "Missing flag -" + std::string(1, param_info.key) +
                    " cannot be defaulted for non-bool/int type without explicit default_value_str.");
            }

            if (param_info.default_value_str) {
                return norb::semantic_cast<DecayedTargetType>(*param_info.default_value_str);
            }

            throw command_registry_error("Missing required argument: -" + std::string(1, param_info.key));
        }

        template <typename OriginalArgsTuple, size_t... Is>
        auto build_decayed_tuple_from_params(const Instruction &inst, const std::vector<ParamInfo> &param_infos,
                                             std::index_sequence<Is...>) const {
            return std::make_tuple(
                get_value_for_param<std::tuple_element_t<Is, OriginalArgsTuple>>(inst, param_infos[Is])...);
        }

      public:
        template <typename Func>
        void register_command(const std::string &command_name, Func target_function,
                              std::initializer_list<ParamInfo> param_infos_list) {
            using Traits = function_traits<Func>;
            using OriginalArgsTuple = typename Traits::arg_tuple_type;
            std::vector<ParamInfo> param_infos_vec(param_infos_list.begin(), param_infos_list.end());

            if (param_infos_vec.size() != Traits::arity) {
                throw command_registry_error("Cmd '" + command_name + "': ParamInfo count " +
                                             std::to_string(param_infos_vec.size()) + " != function arity " +
                                             std::to_string(Traits::arity));
            }

            handlers_[command_name] = [this, target_function, param_infos_vec, command_name](const Instruction &inst) {
                std::apply(target_function, build_decayed_tuple_from_params<OriginalArgsTuple>(
                                                inst, param_infos_vec, std::make_index_sequence<Traits::arity>{}));
            };
        }

        void dispatch(const Instruction &inst) const {
            auto it = handlers_.find(inst.command);
            if (it != handlers_.cend()) {
                it->second(inst);
            } else {
                throw command_registry_error("Unknown command: " + inst.command);
            }
        }
    };
} // namespace ticket