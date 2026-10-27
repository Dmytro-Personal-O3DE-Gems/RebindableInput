
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

    TEST_F(InputChannelTextTest, NumpadDigitsFollowTheRuleAndTheRestComesFromTheTable)
    {
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_numpad_5"), AZStd::string("Num 5"));
        EXPECT_EQ(InputChannelText::GetDisplayText("keyboard_key_numpad_add"), AZStd::string("Num +"));
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
} // namespace RebindableInputTests
