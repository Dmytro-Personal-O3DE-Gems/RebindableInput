
#pragma once

#include <RebindableInput/RebindableInputBus.h>

#include <InputChannelInfo.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>

namespace RebindableInput
{
    //! Waits for the player's next key for one slot ("press a key" in a settings menu) and reports it through
    //! RebindableInputNotificationBus. Owned by RebindableInputSystemComponent, which implements StartCapture with it.
    //!
    //! Listens to raw input above UI (LyShine) and below ImGui, and swallows all input while waiting: Enter or Space must not
    //! press a menu button, and the game must not react. Connected only while capturing or while a key is held that must
    //! stay swallowed.
    class InputCapture
        : private AzFramework::InputChannelEventListener
    {
    public:
        InputCapture();

        //! Escape and gamepad Start: they cancel a capture, so the player cannot bind them.
        static bool IsReservedChannel(AZStd::string_view channelName);

        //! Starts waiting for a channel of this device group and kind. A capture in progress is cancelled first.
        //! Armed on the next tick: the key that started the capture is part of this frame's input.
        void Start(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, ChannelKind kind);

        //! Stops waiting and sends OnCaptureCancelled; nothing happens if no capture is in progress.
        void Cancel();

        bool IsCapturing() const;

    private:
        ////////////////////////////////////////////////////////////////////////
        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;
        ////////////////////////////////////////////////////////////////////////

        struct Target
        {
            AZStd::string m_actionName;
            DeviceGroup m_group = DeviceGroup::KeyboardMouse;
            BindingSlot m_slot = BindingSlot::Primary;
            ChannelKind m_kind = ChannelKind::Button;
        };

        //! Whether the channel's value is far enough from rest to count as the player's choice.
        static bool IsPastThreshold(const AzFramework::InputChannel& inputChannel, ChannelKind kind);

        //! Ends the capture without a notification and disconnects if nothing is left to swallow.
        void StopWaiting();

        void ConnectToInput();
        void DisconnectIfIdle();

        //! The slot being captured; empty when not capturing.
        AZStd::optional<Target> m_target;

        //! False until the tick after Start: until then input passes through.
        bool m_armed = false;

        //! Channels pressed after arming. Only these can be taken: a key held from before (the one that pressed the
        //! menu button) sends Updated events but no Began.
        AZStd::vector<AZStd::string> m_pressedAfterArming;

        //! The taken key and the cancel key: swallowed until released, so the new binding does not fire
        //! at once and a menu does not get the release of Escape.
        AZStd::vector<AZStd::string> m_swallowedUntilReleased;

        bool m_connected = false;

        //! Changes with every Start and Cancel: an arming queued for an older capture does nothing.
        AZ::u32 m_generation = 0;

        //! Exists while this object exists: the queued arming holds a weak_ptr to it.
        AZStd::shared_ptr<int> m_lifetime = AZStd::make_shared<int>(0);
    };

} // namespace RebindableInput
