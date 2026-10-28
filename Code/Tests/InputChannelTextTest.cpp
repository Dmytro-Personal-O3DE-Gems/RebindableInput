
#include <InputChannelText.h>

#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzTest/AzTest.h>

namespace RebindableInputTests
{
    using RebindableInput::InputChannelText;

    class InputChannelTextTest : public UnitTest::LeakDetectionFixture
    {
    protected:
        //! Expects a text for every channel id in the list and reports the channels without one.
        template<class ChannelIds>
        static void ExpectTextForAll(const ChannelIds& channelIds)
        {
            for (const AzFramework::InputChannelId& channelId : channelIds)
            {
                EXPECT_TRUE(InputChannelText::GetDisplayText(channelId.GetName()).has_value())
                    << "No display text for channel '" << channelId.GetName() << "'";
            }
        }
    };

    TEST_F(InputChannelTextTest, LettersAndDigitsAreTheirCharacter)
    {
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_alphanumeric_E"), AZStd::string("E"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_alphanumeric_7"), AZStd::string("7"));
    }

    TEST_F(InputChannelTextTest, FunctionKeysLoseTheLeadingZero)
    {
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_function_F01"), AZStd::string("F1"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_function_F09"), AZStd::string("F9"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_function_F10"), AZStd::string("F10"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_function_F20"), AZStd::string("F20"));
    }

    TEST_F(InputChannelTextTest, WordKeysComeFromTheTable)
    {
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_edit_space"), AZStd::string("Space"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_escape"), AZStd::string("Esc"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_punctuation_comma"), AZStd::string(","));
    }

    TEST_F(InputChannelTextTest, LeftAndRightModifiersShareTheText)
    {
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_modifier_shift_l"), AZStd::string("Shift"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_modifier_shift_r"), AZStd::string("Shift"));
    }

    TEST_F(InputChannelTextTest, NumpadKeysComeFromTheTable)
    {
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_numpad_5"), AZStd::string("Num 5"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_numpad_add"), AZStd::string("Num +"));
    }

    TEST_F(InputChannelTextTest, WordsHaveALocalizationKey)
    {
        const AZStd::optional<RebindableInput::KeyText> space = InputChannelText::GetText("keyboard_key_edit_space");
        ASSERT_TRUE(space.has_value());
        EXPECT_EQ(space->m_key, "input.key.space");
        EXPECT_EQ(space->m_defaultText, "Space");

        // "Num 5" is a phrase: its word order may differ, so each numpad digit has its own key.
        const AZStd::optional<RebindableInput::KeyText> numpad = InputChannelText::GetText("keyboard_key_numpad_5");
        ASSERT_TRUE(numpad.has_value());
        EXPECT_EQ(numpad->m_key, "input.key.numpad_5");
    }

    TEST_F(InputChannelTextTest, LettersDigitsFunctionKeysAndSymbolsAreNotTranslated)
    {
        for (const char* channelName : { "keyboard_key_alphanumeric_E", "keyboard_key_alphanumeric_7",
                 "keyboard_key_function_F01", "keyboard_key_punctuation_comma", "gamepad_button_a" })
        {
            const AZStd::optional<RebindableInput::KeyText> text = InputChannelText::GetText(channelName);
            ASSERT_TRUE(text.has_value()) << channelName;
            EXPECT_TRUE(text->m_key.empty()) << channelName;
        }
    }

    TEST_F(InputChannelTextTest, MouseAndGamepad)
    {
        EXPECT_EQ(InputChannelText::GetDisplayText("mouse_button_left"), AZStd::string("LMB"));
        EXPECT_EQ(InputChannelText::GetDisplayText("gamepad_button_a"), AZStd::string("A"));
        EXPECT_EQ(InputChannelText::GetDisplayText("gamepad_trigger_r2"), AZStd::string("RT"));
    }

    TEST_F(InputChannelTextTest, UnknownNamesHaveNoText)
    {
        EXPECT_FALSE(InputChannelText::GetDisplayText("").has_value());
        EXPECT_FALSE(InputChannelText::GetDisplayText("touch_index_0").has_value());
        // A prefix alone is not a key.
        EXPECT_FALSE(InputChannelText::GetDisplayText("keyboard_key_alphanumeric_").has_value());
        EXPECT_FALSE(InputChannelText::GetDisplayText("keyboard_key_function_F").has_value());
        EXPECT_FALSE(InputChannelText::GetDisplayText("keyboard_key_function_Fxx").has_value());
        EXPECT_FALSE(InputChannelText::GetDisplayText("keyboard_key_numpad_").has_value());
    }

    // If a new engine version adds a channel, these tests list it: add it to the table in InputChannelText.cpp.
    TEST_F(InputChannelTextTest, EveryKeyboardKeyHasText)
    {
        ExpectTextForAll(AzFramework::InputDeviceKeyboard::Key::All);
    }

    TEST_F(InputChannelTextTest, EveryMouseChannelHasText)
    {
        ExpectTextForAll(AzFramework::InputDeviceMouse::Button::All);
        ExpectTextForAll(AzFramework::InputDeviceMouse::Movement::All);
    }

    TEST_F(InputChannelTextTest, EveryGamepadChannelHasText)
    {
        ExpectTextForAll(AzFramework::InputDeviceGamepad::Button::All);
        ExpectTextForAll(AzFramework::InputDeviceGamepad::Trigger::All);
        ExpectTextForAll(AzFramework::InputDeviceGamepad::ThumbStickAxis2D::All);
        ExpectTextForAll(AzFramework::InputDeviceGamepad::ThumbStickAxis1D::All);
        ExpectTextForAll(AzFramework::InputDeviceGamepad::ThumbStickDirection::All);
    }
    TEST_F(InputChannelTextTest, LettersAndPunctuationFollowTheLayout)
    {
        EXPECT_TRUE(InputChannelText::UsesLayoutText("keyboard_key_alphanumeric_Q"));
        EXPECT_TRUE(InputChannelText::UsesLayoutText("keyboard_key_punctuation_semicolon"));
        EXPECT_TRUE(InputChannelText::UsesLayoutText("keyboard_key_supplementary_iso"));

        // Digits keep the digit printed on the key; words follow the game's language; the rest has no layout text.
        EXPECT_FALSE(InputChannelText::UsesLayoutText("keyboard_key_alphanumeric_1"));
        EXPECT_FALSE(InputChannelText::UsesLayoutText("keyboard_key_edit_space"));
        EXPECT_FALSE(InputChannelText::UsesLayoutText("keyboard_key_function_F01"));
        EXPECT_FALSE(InputChannelText::UsesLayoutText("keyboard_key_numpad_5"));
        EXPECT_FALSE(InputChannelText::UsesLayoutText("mouse_button_left"));
    }

    TEST_F(InputChannelTextTest, LayoutTextIsUsedOnlyIfItIsOnePrintableAsciiCharacter)
    {
        EXPECT_EQ(InputChannelText::ChooseLayoutText("Q", "A"), "A");    // AZERTY
        EXPECT_EQ(InputChannelText::ChooseLayoutText(";", "M"), "M");    // AZERTY: a letter where QWERTY has ';'
        EXPECT_EQ(InputChannelText::ChooseLayoutText("ISO", "<"), "<");  // German: the extra key of ISO keyboards

        EXPECT_EQ(InputChannelText::ChooseLayoutText(";", "\xD0\x96"), ";");  // Russian: Cyrillic Zhe (UTF-8)
        EXPECT_EQ(InputChannelText::ChooseLayoutText("[", "ACUTE"), "[");     // a dead key is spelled out
        EXPECT_EQ(InputChannelText::ChooseLayoutText("Q", ""), "Q");          // no layout text (Linux)
        EXPECT_EQ(InputChannelText::ChooseLayoutText("Q", " "), "Q");         // a space is not a key name
    }
} // namespace RebindableInputTests
