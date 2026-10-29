
#pragma once

#include <RebindableInput/RebindableInputBus.h>

#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/functional.h>
#include <AzFramework/Input/Buses/Notifications/InputDeviceNotificationBus.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>

namespace RebindableInput
{
    //! Knows which device group the player uses right now, for input hints and menus.
    //!
    //! - Input switches the group: keyboard keys, mouse buttons and the mouse wheel; gamepad buttons, and triggers and sticks
    //!   past half way (a drifting stick does not switch). Mouse movement switches only once it adds up to a real
    //!   movement within a short time: a bumped desk or a jittering sensor must not change the hints, a player who
    //!   only looks around with the mouse must.
    //! - Before the player's first input, a gamepad that connects switches to the gamepad: a player who starts with a
    //!   gamepad sees gamepad hints on the first screen. (The engine connects gamepads on its first input tick, after
    //!   the system components are active, so this comes as a notification, not as a check in the constructor.)
    //! - The active gamepad disconnecting switches back to keyboard and mouse.
    //!
    //! Listens with the highest priority and never swallows: it must see input that UI, ImGui or a capture swallow.
    class ActiveDeviceTracker
        : private AzFramework::InputChannelEventListener
        , private AzFramework::InputDeviceNotificationBus::Handler
    {
    public:
        //! Called on every change of the group (not for the initial group).
        using ChangedCallback = AZStd::function<void(DeviceGroup group)>;

        explicit ActiveDeviceTracker(ChangedCallback onChanged);
        ~ActiveDeviceTracker() override;

        DeviceGroup GetActiveGroup() const;

    private:
        ////////////////////////////////////////////////////////////////////////
        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AzFramework::InputDeviceNotificationBus::Handler
        void OnInputDeviceConnectedEvent(const AzFramework::InputDevice& inputDevice) override;
        void OnInputDeviceDisconnectedEvent(const AzFramework::InputDevice& inputDevice) override;
        ////////////////////////////////////////////////////////////////////////

        void SetActiveGroup(DeviceGroup group);

        //! Adds mouse movement to the current time window; true once the window holds enough to count as input.
        bool AddMouseTravel(float distance);

        ChangedCallback m_onChanged;
        DeviceGroup m_activeGroup = DeviceGroup::KeyboardMouse;

        //! The player has pressed something: from now on only input decides, not gamepad connections.
        bool m_hadInput = false;

        //! Mouse movement (in sensor counts, X and Y together) since m_mouseTravelStart.
        float m_mouseTravel = 0.0f;
        AZStd::chrono::steady_clock::time_point m_mouseTravelStart;
    };

} // namespace RebindableInput
