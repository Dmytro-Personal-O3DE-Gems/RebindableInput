
#pragma once

#include <RebindableInput/RebindableInputBus.h>

#include <AzCore/std/optional.h>
#include <AzCore/std/string/string_view.h>

namespace RebindableInput
{
    //! What kind of value a channel produces. A key slot can only get a channel of the same kind
    //! as the action's defaults: a camera action that expects mouse movement must not get a key.
    enum class ChannelKind : AZ::u8
    {
        Button,    //!< pressed or not: keys, mouse buttons, gamepad buttons; also triggers and stick directions (0..1)
        MouseAxis, //!< mouse movement or wheel since the last frame
        StickAxis, //!< one axis of a thumbstick, -1..1
        Stick,     //!< a whole thumbstick (2D), used by ThumbstickInputEventMap
    };

    struct ChannelInfo
    {
        AZStd::string_view m_deviceType; //!< as stored in InputEventMap: "keyboard", "mouse" or "gamepad"
        DeviceGroup m_deviceGroup = DeviceGroup::KeyboardMouse;
        ChannelKind m_kind = ChannelKind::Button;
    };

    //! Facts about engine input channels. Stateless, unit tested in Tests/InputChannelInfoTest.cpp.
    //! Built from the engine's own channel lists (InputDeviceKeyboard::Key::All and so on), not from name patterns,
    //! so a name that is not a real channel is never accepted.
    class InputChannelInfo
    {
    public:
        //! Information about a keyboard, mouse or gamepad channel, or an empty optional for any other name.
        static AZStd::optional<ChannelInfo> Find(AZStd::string_view channelName);

        //! The device group of a device type as stored in InputEventMap, or an empty optional for other devices.
        static AZStd::optional<DeviceGroup> GetDeviceGroup(AZStd::string_view deviceType);
    };
} // namespace RebindableInput
