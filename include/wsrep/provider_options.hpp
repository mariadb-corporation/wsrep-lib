/*
 * Copyright (C) 2021 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

/** @file provider_options.hpp
 *
 * A proxy interface to control provider configuration options.
 */

#ifndef WSREP_PROVIDER_OPTIONS_HPP
#define WSREP_PROVIDER_OPTIONS_HPP

#include "provider.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace wsrep
{
    /**
     * Proxy class for managing provider options.
     * Options are strings by default, or flagged with corresponding
     * provider_options::flag as either bool, integer or double.
     */
    class provider_options
    {
    public:
        struct flag
        {
            static const int deprecated = (1 << 0);
            static const int readonly = (1 << 1);
            static const int type_bool = (1 << 2);
            static const int type_integer = (1 << 3);
            static const int type_double = (1 << 4);
        };

        static const int flag_type_mask
            = flag::type_bool | flag::type_integer | flag::type_double;

        class option_value
        {
        public:
            virtual ~option_value() {}
            virtual const char* as_string() const = 0;
            virtual const void* get_ptr() const = 0;
        };

        class option_value_string : public option_value
        {
        public:
            option_value_string(const std::string& value)
                : value_(value)
            {
            }
            ~option_value_string() WSREP_OVERRIDE {}
            const char* as_string() const WSREP_OVERRIDE
            {
                return value_.c_str();
            }
            const void* get_ptr() const WSREP_OVERRIDE
            {
                return value_.c_str();
            }

        private:
            std::string value_;
        };

        class option_value_bool : public option_value
        {
        public:
            option_value_bool(bool value)
                : value_(value)
            {
            }
            ~option_value_bool() WSREP_OVERRIDE {}
            const char* as_string() const WSREP_OVERRIDE
            {
                if (value_)
                {
                    return "yes";
                }
                else
                {
                    return "no";
                }
            }
            const void* get_ptr() const WSREP_OVERRIDE { return &value_; }

        private:
            bool value_;
        };

        class option_value_int : public option_value
        {
        public:
            option_value_int(int64_t value)
                : value_(value)
                , value_str_(std::to_string(value))
            {
            }
            ~option_value_int() WSREP_OVERRIDE {}
            const char* as_string() const WSREP_OVERRIDE
            {
                return value_str_.c_str();
            }
            const void* get_ptr() const WSREP_OVERRIDE { return &value_; }

        private:
            int64_t value_;
            std::string value_str_;
        };

        class option_value_double : public option_value
        {
        public:
            option_value_double(double value)
                : value_(value)
                , value_str_(std::to_string(value))
            {
            }
            ~option_value_double() WSREP_OVERRIDE {}
            const char* as_string() const WSREP_OVERRIDE
            {
                return value_str_.c_str();
            }
            const void* get_ptr() const WSREP_OVERRIDE { return &value_; }

        private:
            double value_;
            std::string value_str_;
        };

        class option
        {
        public:
            option();
            /** Construct option with given values. Allocates
             * memory. */
            option(const std::string& name, std::unique_ptr<option_value> value,
                   std::unique_ptr<option_value> default_value, int flags);
            /** Non copy-constructible. */
            option(const option&) = delete;
            /** Non copy-assignable. */
            option& operator=(const option&) = delete;
            option(option&&) = delete;
            option& operator=(option&&) = delete;
            ~option();

            /**
             * Get name of the option.
             *
             * @return Name of the option.
             */
            const char* name() const { return name_.c_str(); }

            /**
             * Get the real name of the option. This displays the
             * option name as it is visible in provider options.
             */
            const char* real_name() const { return real_name_.c_str(); }

            /**
             * Get value of the option.
             *
             * @return Value of the option.
             */
            option_value* value() const { return value_.get(); }

            /**
             * Get default value of the option.
             *
             * @return Default value of the option.
             */
            option_value* default_value() const { return default_value_.get(); }

            /**
             * Get flags of the option
             *
             * @return Flag of the option
             */
            int flags() const { return flags_; }

            /**
             * Update the value of the option with new_value. The old
             * value is freed.
             */
            void update_value(std::unique_ptr<option_value> new_value);

        private:
            /** Sanitized name with dots replaced with underscores */
            std::string name_;
            /** Real name in provider */
            std::string real_name_;
            std::unique_ptr<option_value> value_;
            std::unique_ptr<option_value> default_value_;
            int flags_;
        };

        provider_options();
        provider_options(const provider_options&) = delete;
        provider_options& operator=(const provider_options&) = delete;

        /**
         * Set initial options. This should be used to initialize
         * provider options after the provider has been loaded.
         * Individual options should be accessed through set()/get().
         *
         * @return Provider status code.
         */
        enum wsrep::provider::status initial_options(wsrep::provider& provider);

        /**
         * Get the option with the given name
         *
         * @param name Name of the option to retrieve
         */
        const option* get_option(const std::string& name) const;

        /**
         * Set a value for the option.
         *
         * @return Wsrep provider status code.
         * @return wsrep::provider::error_size_exceeded if memory could
         *         not be allocated for the new value.
         */
        enum wsrep::provider::status set(wsrep::provider& provider,
                                         const std::string& name,
                                         std::unique_ptr<option_value> value);

        /**
         * Create a new option with default value.
         */
        enum wsrep::provider::status
        set_default(const std::string& name,
                    std::unique_ptr<option_value> value,
                    std::unique_ptr<option_value> default_value, int flags);

        /**
         * Invoke the given function with each provider option
         * as argument.
         *
         * @param fn Function to call for each option
         */
        void for_each(const std::function<void(option*)>& fn) const;

    private:
        using options_map = std::map<std::string, std::unique_ptr<option>>;
        options_map options_;
    };

    /**
     * Equality operator for provider option. Returns true if
     * the name part is equal.
     */
    bool operator==(const wsrep::provider_options::option&,
                    const wsrep::provider_options::option&);
} // namespace wsrep

#endif // WSREP_PROVIDER_OPTIONS_HPP
