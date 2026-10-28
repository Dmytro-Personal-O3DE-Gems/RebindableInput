
#include <ActiveDeviceTracker.h>

#include <InputChannelInfo.h>

#include <AzCore/Math/MathUtils.h>
#include <AzFramework/Input/Channels/InputChannel.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Devices/InputDevice.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

namespace RebindableInput
{
    namespace
    {
        //! Keys and buttons are 1 when pressed; triggers, stick directions, stick axes and whole sticks must pass half way.
        constexpr float SwitchThreshold = 0.5f;
    } // namespace

    ActiveDeviceTracker::ActiveDeviceTracker(ChangedCallback onChanged)
        // First of all listeners, and it never swallows (OnInputChannelEventFiltered returns false).
        : AzFramework::InputChannelEventListener(AzFramework::InputChannelEventListener::GetPriorityFirst(), true)
        , m_onChanged(AZStd::move(onChanged))
    {
        AzFramework::InputDeviceNotificationBus::Handler::BusConnect();
    }

    ActiveDeviceTracker::~ActiveDeviceTracker()
    {
        AzFramework::InputDeviceNotificationBus::Handler::BusDisconnect();
        Disconnect();
    }

    DeviceGroup ActiveDeviceTracker::GetActiveGroup() const
    {
        return m_activeGroup;
    }

    bool ActiveDeviceTracker::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        const AZStd::optional<ChannelInfo> info = InputChannelInfo::Find(inputChannel.GetInputChannelId().GetName());
        if (!info || !inputChannel.IsActive())
        {
            return false; // not a channel of a device group (touch, motion...) or released
        }

        bool counts = false;
        if (info->m_kind == ChannelKind::MouseAxis)
        {
            // Movement does not switch; the wheel does (one event per notch).
            counts = inputChannel.GetInputChannelId() == AzFramework::InputDeviceMouse::Movement::Z && inputChannel.GetValue() != 0.0f;
        }
        else
        {
            counts = AZ::GetAbs(inputChannel.GetValue()) >= SwitchThreshold;
        }

        if (counts)
        {
            m_hadInput = true;
            SetActiveGroup(info->m_deviceGroup);
        }
        return false; // never swallow: this listener only watches
    }

    void ActiveDeviceTracker::OnInputDeviceConnectedEvent(const AzFramework::InputDevice& inputDevice)
    {
        if (!m_hadInput && AzFramework::InputDeviceGamepad::IsGamepadDevice(inputDevice.GetInputDeviceId()))
        {
            SetActiveGroup(DeviceGroup::Gamepad);
        }
    }

    void ActiveDeviceTracker::OnInputDeviceDisconnectedEvent(const AzFramework::InputDevice& inputDevice)
    {
        // TODO: with several gamepads, switch back only when the last one disconnects.
        if (m_activeGroup == DeviceGroup::Gamepad && AzFramework::InputDeviceGamepad::IsGamepadDevice(inputDevice.GetInputDeviceId()))
        {
            SetActiveGroup(DeviceGroup::KeyboardMouse);
        }
    }

    void ActiveDeviceTracker::SetActiveGroup(DeviceGroup group)
    {
        if (group == m_activeGroup)
        {
            return;
        }
        m_activeGroup = group;
        if (m_onChanged)
        {
            m_onChanged(group);
        }
    }

} // namespace RebindableInput
