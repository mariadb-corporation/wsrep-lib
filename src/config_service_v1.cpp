/*
 * Copyright (C) 2022 Codership Oy <info@codership.com>
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

#include "config_service_v1.hpp"
#include "service_helpers.hpp"
#include "v26/wsrep_config_service.h"
#include "wsrep/logger.hpp"
#include "wsrep/provider_options.hpp"

#include <cassert>

namespace wsrep_config_service_v1
{
    wsrep_config_service_v1_t service{ 0 };

    static int map_flags(int flags)
    {
        int option_flags = 0;
        if (flags & WSREP_PARAM_DEPRECATED)
            option_flags |= wsrep::provider_options::flag::deprecated;
        if (flags & WSREP_PARAM_READONLY)
            option_flags |= wsrep::provider_options::flag::readonly;
        if (flags & WSREP_PARAM_TYPE_BOOL)
            option_flags |= wsrep::provider_options::flag::type_bool;
        if (flags & WSREP_PARAM_TYPE_INTEGER)
            option_flags |= wsrep::provider_options::flag::type_integer;
        if (flags & WSREP_PARAM_TYPE_DOUBLE)
            option_flags |= wsrep::provider_options::flag::type_double;
        return option_flags;
    }

    static enum wsrep::provider::status
    make_option(wsrep::provider_options* opt, const char* name, const char* val,
                int flags)
    {
        std::unique_ptr<wsrep::provider_options::option_value> value(
            new wsrep::provider_options::option_value_string(val));
        std::unique_ptr<wsrep::provider_options::option_value> default_value(
            new wsrep::provider_options::option_value_string(val));
        return opt->set_default(name, std::move(value),
                                std::move(default_value), flags);
    }

    static enum wsrep::provider::status
    make_option(wsrep::provider_options* opt, const char* name, int64_t val,
                int flags)
    {
        std::unique_ptr<wsrep::provider_options::option_value> value(
            new wsrep::provider_options::option_value_int(val));
        std::unique_ptr<wsrep::provider_options::option_value> default_value(
            new wsrep::provider_options::option_value_int(val));
        return opt->set_default(name, std::move(value),
                                std::move(default_value), flags);
    }

    static enum wsrep::provider::status
    make_option(wsrep::provider_options* opt, const char* name, bool val,
                int flags)
    {
        std::unique_ptr<wsrep::provider_options::option_value> value(
            new wsrep::provider_options::option_value_bool(val));
        std::unique_ptr<wsrep::provider_options::option_value> default_value(
            new wsrep::provider_options::option_value_bool(val));
        return opt->set_default(name, std::move(value),
                                std::move(default_value), flags);
    }

    static enum wsrep::provider::status
    make_option(wsrep::provider_options* opt, const char* name, double val,
                int flags)
    {
        std::unique_ptr<wsrep::provider_options::option_value> value(
            new wsrep::provider_options::option_value_double(val));
        std::unique_ptr<wsrep::provider_options::option_value> default_value(
            new wsrep::provider_options::option_value_double(val));
        return opt->set_default(name, std::move(value),
                                std::move(default_value), flags);
    }

    wsrep_status_t service_callback(const wsrep_parameter* p, void* context)
    {
        const int flags = map_flags(p->flags);
        enum wsrep::provider::status ret(wsrep::provider::error_unknown);
        wsrep::provider_options* options = (wsrep::provider_options*)context;
        switch (p->flags & WSREP_PARAM_TYPE_MASK)
        {
        case WSREP_PARAM_TYPE_BOOL:
            ret = make_option(options, p->name, p->value.as_bool, flags);
            break;
        case WSREP_PARAM_TYPE_INTEGER:
            ret = make_option(options, p->name, p->value.as_integer, flags);
            break;
        case WSREP_PARAM_TYPE_DOUBLE:
            ret = make_option(options, p->name, p->value.as_double, flags);
            break;
        default:
            assert((p->flags & WSREP_PARAM_TYPE_MASK) == 0);
            ret = make_option(options, p->name, p->value.as_string, flags);
            break;
        }

        if (ret == wsrep::provider::success)
            return WSREP_OK;
        else
            return WSREP_FATAL;
    }
} // namespace wsrep_config_service_v1

static int config_service_v1_probe(void* dlh)
{
    typedef int (*init_fn)(wsrep_config_service_v1_t*);
    typedef void (*deinit_fn)();
    return wsrep_impl::service_probe<init_fn>(
               dlh, WSREP_CONFIG_SERVICE_INIT_FUNC_V1, "config service v1")
           || wsrep_impl::service_probe<deinit_fn>(
               dlh, WSREP_CONFIG_SERVICE_DEINIT_FUNC_V1, "config service v1");
}

static int config_service_v1_init(void* dlh)
{
    typedef int (*init_fn)(wsrep_config_service_v1_t*);
    return wsrep_impl::service_init<init_fn>(
        dlh, WSREP_CONFIG_SERVICE_INIT_FUNC_V1,
        &wsrep_config_service_v1::service, "config service v1");
}

static void config_service_v1_deinit(void* dlh)
{
    typedef int (*deinit_fn)();
    wsrep_impl::service_deinit<deinit_fn>(
        dlh, WSREP_CONFIG_SERVICE_DEINIT_FUNC_V1, "config service v1");
}

int wsrep::config_service_v1_fetch(wsrep::provider& provider,
                                   wsrep::provider_options* options)
{
    struct wsrep_st* wsrep = (struct wsrep_st*)provider.native();
    if (wsrep == nullptr)
    {
        // Not a provider which was loaded via wsrep-API
        return 0;
    }
    if (config_service_v1_probe(wsrep->dlh))
    {
        wsrep::log_warning() << "Provider does not support config service v1";
        wsrep::log_warning() << "Provider " << provider.name() << " " <<
            provider.version() << " used required at least 26.4.14";
        return 1;
    }
    if (config_service_v1_init(wsrep->dlh))
    {
        wsrep::log_warning() << "Failed to initialize config service v1";
        return 1;
    }

    wsrep_status_t ret = wsrep_config_service_v1::service.get_parameters(
        wsrep, &wsrep_config_service_v1::service_callback, options);

    config_service_v1_deinit(wsrep->dlh);

    if (ret != WSREP_OK)
    {
        return 1;
    }

    return 0;
}
