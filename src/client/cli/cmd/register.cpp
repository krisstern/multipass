/*
 * Copyright (C) 2021 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "register.h"

#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/prompters.h>
#include <multipass/exceptions/cli_exceptions.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Register::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::AuthenticateReply& reply) { return mp::ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::authenticate, request, on_success, on_failure);
}

std::string cmd::Register::name() const
{
    return "register";
}

std::vector<std::string> cmd::Register::aliases() const
{
    return {name(), "authenticate"};
}

QString cmd::Register::short_help() const
{
    return QStringLiteral("Register client");
}

QString cmd::Register::description() const
{
    return QStringLiteral("Register the client for allowing connections to the Multipass service.");
}

mp::ParseCode cmd::Register::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("passphrase",
                                  "Trusted passphrase to send to the Multipass service. If omitted, a prompt will be "
                                  "displayed for entering the passphrase.",
                                  "<passphrase>");

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    if (parser->positionalArguments().count() > 1)
    {
        cerr << "Too many passphrases given\n";
        return ParseCode::CommandLineError;
    }

    if (parser->positionalArguments().empty())
    {
        if (!term->is_live())
        {
            cerr << "The terminal is not live: The passphrase argument is required\n";
            return ParseCode::CommandLineError;
        }

        try
        {
            mp::PassphrasePrompter prompter(term);
            auto passphrase = prompter.prompt("Please enter passphrase: ");

            if (passphrase.empty())
            {
                cerr << "No passphrase given\n";
                return ParseCode::CommandLineError;
            }

            request.set_passphrase(passphrase);
        }
        catch (const mp::PromptException& e)
        {
            cerr << e.what() << std::endl;
            return ParseCode::CommandLineError;
        }
    }
    else
    {
        request.set_passphrase(parser->positionalArguments().first().toStdString());
    }

    return status;
}
