#pragma once

#include <any>
#include <functional>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "stlite/map.hpp"
#include "stlite/vector.hpp"
#include "utility/decorators.hpp"
#include "semantic_cast.hpp"
#include "utils.hpp"
#include "ntraits.hpp"

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

        template <typename WrappedCallable, typename OstreamLike>
        struct function_traits_impl<norb::PrintResult<WrappedCallable, OstreamLike>>
            : function_traits_impl<std::decay_t<WrappedCallable>> {}; // Recurse on the wrapped type

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
    } // namespace detail_traits
    template <typename T> using function_traits = detail_traits::function_traits_impl<std::decay_t<T>>;

    class CommandRegistry {
      public:
        struct FlagIndicator {};
        static constexpr FlagIndicator is_flag{};

        struct ParamInfo {
            char key;
            bool is_flag;
            std::any default_value; // Can store a default value of any type

            // Constructor for: {'k'} (required, no default)
            ParamInfo(char k) : key(k), is_flag(false), default_value() {
            } // default_value is empty

            // Constructor for: {'k', flag}
            ParamInfo(char k, FlagIndicator) : key(k), is_flag(true), default_value() {
            }

            // Constructor for: {'k', default_typed_value}
            // e.g., {'p', 1}, {'s', "hello"}, {'o', MyCustomType{}}, {'x', std::nullopt}
            template <typename T> ParamInfo(char k, T &&val) : key(k), is_flag(false) {
                // If T is FlagIndicator, it should have been caught by the (char, FlagIndicator) constructor.
                default_value = std::make_any<std::decay_t<T>>(std::forward<T>(val));
            }
        };

      private:
        norb::map<std::string, std::function<void(const Instruction &)>> handlers_;

        template <typename TargetTypeOriginal>
        std::decay_t<TargetTypeOriginal> get_value_for_param(const Instruction &inst,
                                                             const ParamInfo &param_info) const {
            using DecayedTargetType = std::decay_t<TargetTypeOriginal>;
            bool key_found = false;
            std::string kwarg_val_str; // String value from instruction's kwargs if key is found

            // 1. Check if the key is present in the parsed instruction's arguments
            for (const auto &kwarg : inst.kwargs) {
                if (kwarg.first == param_info.key) {
                    key_found = true;
                    kwarg_val_str = kwarg.second; // kwarg.second is std::string
                    break;
                }
            }

            // 2. If key was found in the instruction:
            if (key_found) {
                if (param_info.is_flag) {
                    // If the parameter is a flag and the key was found, it means the flag is present.
                    if constexpr (std::is_same_v<DecayedTargetType, bool>)
                        return true;
                    if constexpr (std::is_same_v<DecayedTargetType, int>)
                        return 1;
                }
                return norb::semantic_cast<DecayedTargetType>(kwarg_val_str);
            }

            // 3. Key was NOT found in the instruction. Handle defaults or missing required args.
            if (param_info.is_flag) {
                if constexpr (std::is_same_v<DecayedTargetType, bool>)
                    return false;
                if constexpr (std::is_same_v<DecayedTargetType, int>)
                    return 0;
                throw std::logic_error("Error for flag -" + std::string(1, param_info.key) +
                                       ": Missing flag cannot be meaningfully defaulted for target type " +
                                       typeid(DecayedTargetType).name() + " without an explicit default value.");
            }

            if (param_info.default_value.has_value()) {
                const std::any &default_val_any = param_info.default_value;

                // Try to cast the std::any to the DecayedTargetType directly.
                if (const DecayedTargetType *ptr = std::any_cast<DecayedTargetType>(&default_val_any)) {
                    return *ptr;
                }

                // Specific handling if the target is std::optional and the default was std::nullopt
                if constexpr (norb::is_optional_v<DecayedTargetType>) {
                    if (std::any_cast<std::nullopt_t>(&default_val_any)) {
                        return std::nullopt; // DecayedTargetType is std::optional<Something>
                    }
                }

                // If the std::any holds a string (e.g., from ParamInfo('k', "123") or ParamInfo('k', 123)),
                // try to semantic_cast this string to the target type.
                if (const std::string *str_ptr = std::any_cast<std::string>(&default_val_any)) {
                    return norb::semantic_cast<DecayedTargetType>(*str_ptr);
                }
                if (const char *const *cstr_ptr_ptr = std::any_cast<const char *>(&default_val_any)) {
                    if (*cstr_ptr_ptr) {
                        return norb::semantic_cast<DecayedTargetType>(std::string(*cstr_ptr_ptr));
                    }
                }

                // Add more specific conversions from types stored in std::any if necessary.
                if constexpr (std::is_integral_v<DecayedTargetType> || std::is_floating_point_v<DecayedTargetType>) {
                    if (const int *int_ptr = std::any_cast<int>(&default_val_any)) {
                        if constexpr (std::is_convertible_v<int, DecayedTargetType>) {
                            return static_cast<DecayedTargetType>(*int_ptr);
                        }
                    }
                    if (const double *double_ptr = std::any_cast<double>(&default_val_any)) {
                        if constexpr (std::is_convertible_v<double, DecayedTargetType>) {
                            return static_cast<DecayedTargetType>(*double_ptr);
                        }
                    }
                    // Add for bool if stored directly as bool in std::any
                    if (const bool *bool_ptr = std::any_cast<bool>(&default_val_any)) {
                        if constexpr (std::is_convertible_v<bool, DecayedTargetType>) {
                            return static_cast<DecayedTargetType>(*bool_ptr);
                        }
                    }
                }
                throw std::runtime_error("Type mismatch for default value of key -" + std::string(1, param_info.key) +
                                         ". Stored default type in std::any ('" + default_val_any.type().name() +
                                         "') is not directly usable or convertible to target type '" +
                                         typeid(DecayedTargetType).name() + "'.");

            } else {
                // If the target type is std::optional, and no key was found, and no default_value was specified,
                // it should become std::nullopt.
                if constexpr (norb::is_optional_v<DecayedTargetType>) {
                    return std::nullopt;
                } else {
                    // Not a flag, key not found, not optional, and no default value -> missing required argument.
                    throw std::runtime_error("Missing required argument: -" + std::string(1, param_info.key) +
                                             " (and no default value was specified).");
                }
            }
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