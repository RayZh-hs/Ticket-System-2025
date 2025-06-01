// logging.hpp
#pragma once

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility> // For std::move

namespace norb {
    // Enum for log levels
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

    // Helper function to convert LogLevel to string
    inline std::string logLevelToString(LogLevel level) {
        switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    class Logger {
      private:
        std::ofstream file_stream_;
        LogLevel current_level_ = LogLevel::INFO;
        bool prefix_pending_ = false;
        inline static int current_line_number_ = 0;

        // Helper to print the prefix
        void print_prefix_if_needed() {
            if (prefix_pending_) {
                std::string level_str = logLevelToString(current_level_);
                std::string prefix_str = "[" + std::to_string(current_line_number_) + "] [" + level_str + "] ";

                std::clog << prefix_str;
                if (file_stream_.is_open() && file_stream_.good()) {
                    file_stream_ << prefix_str;
                }
                prefix_pending_ = false;
            }
        }

      public:
        explicit Logger(const std::filesystem::path &file_path) {
            file_stream_.open(file_path, std::ios::app);
            if (!file_stream_.is_open()) {
                std::string error_message = "Failed to open log file: ";
                try {
                    error_message += file_path.string();
                } catch (const std::exception &e) {
                    error_message += "[unrepresentable path]";
                }
                throw std::runtime_error(error_message);
            }
            file_stream_ << "\n-- Started new logging session --\n";
        }

        ~Logger() {
            if (file_stream_.is_open()) {
                file_stream_.flush();
                file_stream_.close();
            }
        }

        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger(Logger &&other) noexcept
            : file_stream_(std::move(other.file_stream_)), current_level_(other.current_level_),
              prefix_pending_(other.prefix_pending_) {
        }

        Logger &operator=(Logger &&other) noexcept {
            if (this != &other) {
                if (file_stream_.is_open()) {
                    file_stream_.flush();
                    file_stream_.close();
                }
                file_stream_ = std::move(other.file_stream_);
                current_level_ = other.current_level_;
                prefix_pending_ = other.prefix_pending_;
            }
            return *this;
        }

        static void set_line_number(int line_number) {
            current_line_number_ = line_number;
        }

        Logger &as(LogLevel level) {
            current_level_ = level;
            prefix_pending_ = true;
            return *this;
        }

        template <typename T> Logger &operator<<(const T &message) {
            print_prefix_if_needed();
            std::clog << message; // Output to standard log (console)
            if (file_stream_.is_open() && file_stream_.good()) {
                file_stream_ << message; // Output to file
            }
            return *this;
        }

        Logger &operator<<(std::ostream &(*manip)(std::ostream &)) {
            print_prefix_if_needed(); // Print prefix if this is the first part of a new log entry
            manip(std::clog);
            if (file_stream_.is_open() && file_stream_.good()) {
                manip(file_stream_);
            }
            return *this;
        }

        Logger &operator<<(std::ios_base &(*manip)(std::ios_base &)) {
            print_prefix_if_needed();
            manip(std::clog);
            if (file_stream_.is_open() && file_stream_.good()) {
                manip(file_stream_);
            }
            return *this;
        }
    };

    // Stub implementation for Logger
    class NoLogger {
      public:
        explicit NoLogger(const std::filesystem::path &) {
        }

        ~NoLogger() = default;

        NoLogger(const NoLogger &) = delete;
        NoLogger &operator=(const NoLogger &) = delete;
        NoLogger(NoLogger &&other) noexcept = default;

        NoLogger &operator=(NoLogger &&other) noexcept {
            return *this;
        }

        static void set_line_number(int line_number) {
        }

        NoLogger &as(LogLevel level) {
            return *this;
        }

        template <typename T> NoLogger &operator<<(const T &) {
            return *this;
        }

        NoLogger &operator<<(std::ostream &(*)(std::ostream &)) {
            return *this;
        }

        NoLogger &operator<<(std::ios_base &(*)(std::ios_base &)) {
            return *this;
        }
    };

} // namespace norb