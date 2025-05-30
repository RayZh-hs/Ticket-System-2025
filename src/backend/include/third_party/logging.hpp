// logging.hpp
#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace norb
{
    class logger
    {
    private:
        std::ofstream file_stream_;

    public:
        // Constructor: opens the log file
        explicit logger(const std::filesystem::path& file_path)
        {
            file_stream_.open(file_path, std::ios::app);
            if (!file_stream_.is_open())
            {
                std::string error_message = "Failed to open log file: ";
                try
                {
                    error_message += file_path.string();
                }
                catch (const std::exception& e)
                {
                    error_message += "[unrepresentable path]";
                }
                throw std::runtime_error(error_message);
            }
        }

        // Destructor: ensures the file stream is flushed and closed
        ~logger()
        {
            if (file_stream_.is_open())
            {
                file_stream_.flush(); // Ensure all buffered output is written
                file_stream_.close();
            }
        }

        logger(const logger&) = delete;
        logger& operator=(const logger&) = delete;

        logger(logger&& other) noexcept
            : file_stream_(std::move(other.file_stream_))
        {
        }

        logger& operator=(logger&& other) noexcept
        {
            if (this != &other)
            {
                // Close our own stream if open, before taking over the other's
                if (file_stream_.is_open())
                {
                    file_stream_.flush();
                    file_stream_.close();
                }
                file_stream_ = std::move(other.file_stream_);
                // log_file_path_ = std::move(other.log_file_path_);
            }
            return *this;
        }

        // Overload operator<< for general types
        template <typename T>
        logger& operator<<(const T& message)
        {
            std::clog << message; // Output to standard log (console)
            if (file_stream_.is_open() && file_stream_.good())
            {
                file_stream_ << message; // Output to file
            }
            return *this;
        }

        logger& operator<<(std::ostream& (*manip)(std::ostream&))
        {
            manip(std::clog);
            if (file_stream_.is_open() && file_stream_.good())
            {
                manip(file_stream_);
            }
            return *this;
        }

        logger& operator<<(std::ios_base& (*manip)(std::ios_base&))
        {
            manip(std::clog);
            if (file_stream_.is_open() && file_stream_.good())
            {
                manip(file_stream_);
            }
            return *this;
        }
    };
} // namespace norb
