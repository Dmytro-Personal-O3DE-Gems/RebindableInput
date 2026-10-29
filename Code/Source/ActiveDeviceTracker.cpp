
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

        //! Mouse movement switches once it adds up to this within MouseTravelWindow. The deltas are raw sensor counts
        //! (WM_INPUT on Windows), so the distance depends on the mouse's DPI: 150 counts are about 5 mm at 800 DPI and
        //! 2.4 mm at 1600 DPI. A bump or jitter stays below that, a deliberate look around passes it in a fraction of
        //! a second. TODO: tune with more mice.
        constexpr float MouseTravelThreshold = 150.0f;
        constexpr AZStd::chrono::milliseconds MouseTravelWindow{ 200 };
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
            if (inputChannel.GetInputChannelId() == AzFramework::InputDeviceMouse::Movement::Z)
            {
                counts = inputChannel.GetValue() != 0.0f; // the wheel: one event per notch, always deliberate
            }
            else
            {
                counts = AddMouseTravel(AZ::GetAbs(inputChannel.GetValue()));
            }
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

    bool ActiveDeviceTracker::AddMouseTravel(float distance)
    {
        if (m_activeGroup == DeviceGroup::KeyboardMouse)
        {
            return false; // already there: nothing to switch, and no travel to carry over
        }

        // A fixed window from the first movement: jitter spread over minutes never adds up.
        const AZStd::chrono::steady_clock::time_point now = AZStd::chrono::steady_clock::now();
        if (now - m_mouseTravelStart > MouseTravelWindow)
        {
            m_mouseTravel = 0.0f;
            m_mouseTravelStart = now;
        }
        m_mouseTravel += distance;
        return m_mouseTravel >= MouseTravelThreshold;
    }

    void ActiveDeviceTracker::SetActiveGroup(DeviceGroup group)
    {
        if (group == m_activeGroup)
        {
            return;
        }
        m_activeGroup = group;
        m_mouseTravel = 0.0f; // movement from before the switch must not count towards the next one
        if (m_onChanged)
        {
            m_onChanged(group);
        }
    }

} // namespace RebindableInput
