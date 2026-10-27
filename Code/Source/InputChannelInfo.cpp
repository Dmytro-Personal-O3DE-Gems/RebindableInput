
#include <InputChannelInfo.h>

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

namespace RebindableInput
{
    namespace
    {
        template<class ChannelIds>
        bool Contains(const ChannelIds& channelIds, AZStd::string_view channelName)
        {
            for (const AzFramework::InputChannelId& channelId : channelIds)
            {
                if (channelName == channelId.GetName())
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    AZStd::optional<ChannelInfo> InputChannelInfo::Find(AZStd::string_view channelName)
    {
        using AzFramework::InputDeviceGamepad;
        using AzFramework::InputDeviceKeyboard;
        using AzFramework::InputDeviceMouse;

        // The names point into the engine's static channel and device ids, so the views stay valid.
        const AZStd::string_view keyboard = InputDeviceKeyboard::Id.GetName();
        const AZStd::string_view mouse = InputDeviceMouse::Id.GetName();
        const AZStd::string_view gamepad = InputDeviceGamepad::Name;

        if (Contains(InputDeviceKeyboard::Key::All, channelName))
        {
            return ChannelInfo{ keyboard, DeviceGroup::KeyboardMouse, ChannelKind::Button };
        }
        if (Contains(InputDeviceMouse::Button::All, channelName))
        {
            return ChannelInfo{ mouse, DeviceGroup::KeyboardMouse, ChannelKind::Button };
        }
        if (Contains(InputDeviceMouse::Movement::All, channelName))
        {
            return ChannelInfo{ mouse, DeviceGroup::KeyboardMouse, ChannelKind::MouseAxis };
        }
        // Triggers and stick directions are analog (0..1) but work as buttons: "RT to jump" is common.
        if (Contains(InputDeviceGamepad::Button::All, channelName) ||
            Contains(InputDeviceGamepad::Trigger::All, channelName) ||
            Contains(InputDeviceGamepad::ThumbStickDirection::All, channelName))
        {
            return ChannelInfo{ gamepad, DeviceGroup::Gamepad, ChannelKind::Button };
        }
        if (Contains(InputDeviceGamepad::ThumbStickAxis1D::All, channelName))
        {
            return ChannelInfo{ gamepad, DeviceGroup::Gamepad, ChannelKind::StickAxis };
        }
        if (Contains(InputDeviceGamepad::ThumbStickAxis2D::All, channelName))
        {
            return ChannelInfo{ gamepad, DeviceGroup::Gamepad, ChannelKind::Stick };
        }
        return AZStd::nullopt;
    }

    AZStd::optional<DeviceGroup> InputChannelInfo::GetDeviceGroup(AZStd::string_view deviceType)
    {
        if (deviceType == AzFramework::InputDeviceKeyboard::Id.GetName() || deviceType == AzFramework::InputDeviceMouse::Id.GetName())
        {
            return DeviceGroup::KeyboardMouse;
        }
        if (deviceType == AzFramework::InputDeviceGamepad::Name)
        {
            return DeviceGroup::Gamepad;
        }
        return AZStd::nullopt;
    }
} // namespace RebindableInput
