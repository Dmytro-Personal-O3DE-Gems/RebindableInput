
#pragma once

#include <RebindableInput/RebindableInputBus.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>

namespace RebindableInput
{
    //! Notifications of the rebinding API (RebindableInputRequests).
    //! Changes of the bindings themselves are announced through InputHintsContractNotifications::OnInputHintsChanged.
    class RebindableInputNotifications
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        virtual ~RebindableInputNotifications() = default;

        //! The result of RebindableInputRequests::SaveBindings. Sent on the main thread.
        virtual void OnBindingsSaved([[maybe_unused]] bool success) {}

        //! RebindableInputRequests::StartCapture got the player's key for the slot. Nothing is bound yet.
        virtual void OnChannelCaptured(
            [[maybe_unused]] const AZStd::string& actionName,
            [[maybe_unused]] DeviceGroup group,
            [[maybe_unused]] BindingSlot slot,
            [[maybe_unused]] const AZStd::string& channelName) {}

        //! The player switched between keyboard and mouse and gamepad. Input hints are already updated
        //! (InputHintsContractNotifications::OnInputHintsChanged is sent as well).
        virtual void OnActiveDeviceGroupChanged([[maybe_unused]] DeviceGroup group) {}

        //! The capture for the slot ended without a key: Escape, gamepad Start, CancelCapture or a new StartCapture.
        virtual void OnCaptureCancelled(
            [[maybe_unused]] const AZStd::string& actionName,
            [[maybe_unused]] DeviceGroup group,
            [[maybe_unused]] BindingSlot slot) {}
    };

    using RebindableInputNotificationBus = AZ::EBus<RebindableInputNotifications>;

} // namespace RebindableInput
