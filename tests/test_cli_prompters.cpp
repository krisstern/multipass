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

#include "common.h"
#include "mock_terminal.h"
#include "stub_terminal.h"

#include <multipass/cli/prompters.h>
#include <multipass/exceptions/cli_exceptions.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct CLIPrompters : Test
{
    std::ostringstream cerr, cout;
    std::istringstream cin;
    mpt::StubTerminal term{cout, cerr, cin};
};

TEST_F(CLIPrompters, PlainPromptsText)
{
    auto prompt = mp::PlainPrompter{&term};
    cin.str("\n");
    prompt.prompt("foo");

    EXPECT_EQ(cout.str(), "foo: ");
}

TEST_F(CLIPrompters, PlainReturnsText)
{
    auto prompt = mp::PlainPrompter{&term};
    cin.str("value\n");

    EXPECT_EQ(prompt.prompt(""), "value");
}

// Note that the following test does not actually test if the terminal echos or not-
// that is specific to platform terminal types.
TEST_F(CLIPrompters, passphraseCallsEchoAndReturnsExpectedPassphrase)
{
    const std::string prompter_string{"Enter passphrase: "};
    const std::string passphrase{"foo"};
    mpt::MockTerminal mock_terminal;

    cin.str(passphrase + "\n");

    EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
    EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
    EXPECT_CALL(mock_terminal, cout()).Times(2).WillRepeatedly([this]() -> std::ostream& { return cout; });
    EXPECT_CALL(mock_terminal, cin()).Times(2).WillRepeatedly([this]() -> std::istream& { return cin; });

    mp::PassphrasePrompter prompter{&mock_terminal};

    auto input = prompter.prompt(prompter_string);

    EXPECT_EQ(cout.str(), prompter_string + "\n");
    EXPECT_EQ(input, passphrase);
}

class CLIPromptersBadCinState : public CLIPrompters, public WithParamInterface<std::ios_base::iostate>
{
};

TEST_P(CLIPromptersBadCinState, PlainThrows)
{
    auto prompt = mp::PlainPrompter{&term};

    cin.clear(GetParam());
    MP_EXPECT_THROW_THAT(prompt.prompt(""), mp::PromptException, mpt::match_what(HasSubstr("Failed to read value")));
}

INSTANTIATE_TEST_SUITE_P(CLIPrompters, CLIPromptersBadCinState,
                         Values(std::ios::eofbit, std::ios::failbit, std::ios::badbit));
