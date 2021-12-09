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
    EXPECT_CALL(mock_terminal, cin()).WillOnce([this]() -> std::istream& { return cin; });

    mp::PassphrasePrompter prompter{&mock_terminal};

    auto input = prompter.prompt(prompter_string);

    EXPECT_EQ(cout.str(), prompter_string + "\n");
    EXPECT_EQ(input, passphrase);
}

TEST_F(CLIPrompters, newPassPhraseCallsEchoAndReturnsExpectedPassphrase)
{
    const std::string prompter1_string{"Enter passphrase: "}, prompter2_string{"Re-enter passphrase: "};
    const std::string passphrase{"foo"};
    mpt::MockTerminal mock_terminal;

    EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
    EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
    EXPECT_CALL(mock_terminal, cout()).Times(3).WillRepeatedly([this]() -> std::ostream& { return cout; });
    EXPECT_CALL(mock_terminal, cin()).Times(2).WillRepeatedly([this, &passphrase]() -> std::istream& {
        cin.str(passphrase + "\n");
        return cin;
    });

    mp::NewPassphrasePrompter prompter{&mock_terminal};

    auto input = prompter.prompt(prompter1_string, prompter2_string);

    EXPECT_EQ(cout.str(), prompter1_string + "\n" + prompter2_string + "\n");
    EXPECT_EQ(input, passphrase);
}

TEST_F(CLIPrompters, newPassPhraseWrongPassphraseRepeats)
{
    const std::string prompter1_string{"Enter passphrase: "}, prompter2_string{"Re-enter passphrase: "};
    const std::string passphrase{"foo"}, wrong_passphrase{"bar"};
    mpt::MockTerminal mock_terminal;

    EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
    EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);

    auto good_passphrase = [this, &passphrase]() -> std::istream& {
        cin.str(passphrase + "\n");
        return cin;
    };

    auto bad_passphrase = [this, &wrong_passphrase]() -> std::istream& {
        cin.str(wrong_passphrase + "\n");
        return cin;
    };

    EXPECT_CALL(mock_terminal, cout()).Times(6).WillRepeatedly([this]() -> std::ostream& { return cout; });
    EXPECT_CALL(mock_terminal, cin())
        .Times(4)
        .WillOnce(Invoke(good_passphrase))
        .WillOnce(Invoke(bad_passphrase))
        .WillRepeatedly(Invoke(good_passphrase));

    mp::NewPassphrasePrompter prompter{&mock_terminal};

    auto input = prompter.prompt(prompter1_string, prompter2_string);

    EXPECT_EQ(cout.str(), prompter1_string + "\n" + prompter2_string +
                              "\nPassphrases do not match. Please try again.\n" + prompter1_string + "\n" +
                              prompter2_string + "\n");
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
