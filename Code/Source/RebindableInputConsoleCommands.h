
#pragma once

#include <RebindableInput/RebindableInputNotificationBus.h>

#include <AzCore/Component/TickBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/string/string.h>

namespace RebindableInput
{
    //! Debug console commands for the rebinding API:
    //!   rebind_list [action]
    //!   rebind_set <action> <KeyboardMouse|km|Gamepad|gp> <Primary|p|Alternate|a> <channel> [Move|Duplicate]
    //!   rebind_clear <action> <group> <slot>
    //!   rebind_reset [action]
    //!   rebind_save
    //!   rebind_revert
    //!   rebind_capture <action> <group> <slot> [delay seconds, 0 by default]
    //! They call RebindableInputInterface the way a settings menu would, so they test the public API.
    //!
    //! The commands are members, not AZ_CONSOLEFREEFUNC globals: the gem code is a static library, and the linker
    //! drops an object file that nothing references, global console commands included.
    //! They exist (are registered in the console) while this object exists: RebindableInputSystemComponent creates it
    //! in Activate and destroys it in Deactivate.
    class RebindableInputConsoleCommands
        : private AZ::TickBus::Handler
        , private RebindableInputNotificationBus::Handler
    {
    public:
        RebindableInputConsoleCommands();
        ~RebindableInputConsoleCommands() override;

        //! The instance the command functions reach (they are plain functions); nullptr while none exists.
        static RebindableInputConsoleCommands* GetInstance();

        //! rebind_capture: starts StartCapture, optionally after a delay. When the key arrives, it is bound with
        //! ConflictMode::Move, the way a settings menu would after its pop-up.
        //! In the game the console is ImGui's: while it is open it swallows all input, and the Home that hides it is not
        //! swallowed. So a captured Home (ImGui's default key) restarts the capture instead of being bound.
        void ScheduleCapture(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, float delaySeconds);

    private:
        ////////////////////////////////////////////////////////////////////////
        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // RebindableInputNotificationBus::Handler
        void OnChannelCaptured(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName) override;
        void OnCaptureCancelled(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) override;
        ////////////////////////////////////////////////////////////////////////

        struct PendingCapture
        {
            AZStd::string m_actionName;
            DeviceGroup m_group = DeviceGroup::KeyboardMouse;
            BindingSlot m_slot = BindingSlot::Primary;
        };

        //! A capture waiting for its delay; the tick handler is connected only while it exists.
        AZStd::optional<PendingCapture> m_pendingCapture;
        float m_secondsLeft = 0.0f;

        //! The capture in progress was started by rebind_capture: only then its result is bound and printed.
        bool m_captureStartedHere = false;

        AZ::ConsoleFunctor<void, false> m_list;
        AZ::ConsoleFunctor<void, false> m_set;
        AZ::ConsoleFunctor<void, false> m_clear;
        AZ::ConsoleFunctor<void, false> m_reset;
        AZ::ConsoleFunctor<void, false> m_save;
        AZ::ConsoleFunctor<void, false> m_revert;
        AZ::ConsoleFunctor<void, false> m_capture;
    };

} // namespace RebindableInput
