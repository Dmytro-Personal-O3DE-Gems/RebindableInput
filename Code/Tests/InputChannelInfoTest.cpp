
#include <InputChannelInfo.h>

#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace RebindableInputTests
{
    using RebindableInput::ChannelInfo;
    using RebindableInput::ChannelKind;
    using RebindableInput::DeviceGroup;
    using RebindableInput::InputChannelInfo;

    class InputChannelInfoTest : public UnitTest::LeakDetectionFixture
    {
    protected:
        static void ExpectInfo(const char* channelName, const char* deviceType, DeviceGroup group, ChannelKind kind)
        {
            const AZStd::optional<ChannelInfo> info = InputChannelInfo::Find(channelName);
            ASSERT_TRUE(info.has_value()) << channelName;
            EXPECT_EQ(info->m_deviceType, AZStd::string_view(deviceType)) << channelName;
            EXPECT_EQ(info->m_deviceGroup, group) << channelName;
            EXPECT_EQ(info->m_kind, kind) << channelName;
        }
    };

    TEST_F(InputChannelInfoTest, KeyboardKeysAreButtons)
    {
        ExpectInfo("keyboard_key_alphanumeric_E", "keyboard", DeviceGroup::KeyboardMouse, ChannelKind::Button);
        ExpectInfo("keyboard_key_edit_space", "keyboard", DeviceGroup::KeyboardMouse, ChannelKind::Button);
    }

    TEST_F(InputChannelInfoTest, MouseButtonsAreButtonsAndMovementIsAnAxis)
    {
        ExpectInfo("mouse_button_left", "mouse", DeviceGroup::KeyboardMouse, ChannelKind::Button);
        ExpectInfo("mouse_delta_x", "mouse", DeviceGroup::KeyboardMouse, ChannelKind::MouseAxis);
        ExpectInfo("mouse_delta_z", "mouse", DeviceGroup::KeyboardMouse, ChannelKind::MouseAxis);
    }

    TEST_F(InputChannelInfoTest, GamepadButtonsTriggersAndStickDirectionsAreButtons)
    {
        ExpectInfo("gamepad_button_a", "gamepad", DeviceGroup::Gamepad, ChannelKind::Button);
        ExpectInfo("gamepad_trigger_r2", "gamepad", DeviceGroup::Gamepad, ChannelKind::Button);
        ExpectInfo("gamepad_thumbstick_l_up", "gamepad", DeviceGroup::Gamepad, ChannelKind::Button);
    }

    TEST_F(InputChannelInfoTest, StickAxesAndWholeSticks)
    {
        ExpectInfo("gamepad_thumbstick_l_x", "gamepad", DeviceGroup::Gamepad, ChannelKind::StickAxis);
        ExpectInfo("gamepad_thumbstick_r", "gamepad", DeviceGroup::Gamepad, ChannelKind::Stick);
    }

    TEST_F(InputChannelInfoTest, OtherNamesAreUnknown)
    {
        EXPECT_FALSE(InputChannelInfo::Find("").has_value());
        EXPECT_FALSE(InputChannelInfo::Find("keyboard_key_not_a_key").has_value());
        EXPECT_FALSE(InputChannelInfo::Find("mouse_system_cursor_position").has_value()); // a position, not a key
        EXPECT_FALSE(InputChannelInfo::Find("touch_index_0").has_value());
    }

    TEST_F(InputChannelInfoTest, DeviceGroupsOfDeviceTypes)
    {
        EXPECT_EQ(InputChannelInfo::GetDeviceGroup("keyboard"), DeviceGroup::KeyboardMouse);
        EXPECT_EQ(InputChannelInfo::GetDeviceGroup("mouse"), DeviceGroup::KeyboardMouse);
        EXPECT_EQ(InputChannelInfo::GetDeviceGroup("gamepad"), DeviceGroup::Gamepad);
        EXPECT_FALSE(InputChannelInfo::GetDeviceGroup("touch").has_value());
    }
} // namespace RebindableInputTests
