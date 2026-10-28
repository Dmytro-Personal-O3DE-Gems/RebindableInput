
#pragma once

#include <RebindableInput/RebindableInputTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/containers/vector.h>

namespace RebindableInput
{
    //! Groups of input devices. Every input action has its own key slots in each group,
    //! e.g. Interact = E (keyboard and mouse) and A (gamepad).
    enum class DeviceGroup : AZ::u8
    {
        KeyboardMouse = 0,
        Gamepad = 1,
    };
    inline constexpr size_t DeviceGroupCount = 2;

    //! Key slots of an input action within one device group.
    //! The defaults come from the order of the bindings in the .keybinds file: the first binding of a device group
    //! is the primary one, the second the alternate one. More than two are disabled with a warning.
    enum class BindingSlot : AZ::u8
    {
        Primary = 0,
        Alternate = 1,
    };
    inline constexpr size_t BindingSlotCount = 2;

    //! What is in a key slot.
    //! The text for the player is not translated by this gem; read m_key and m_defaultText like
    //! InputHintsContract::InputHint: an empty key means "show m_defaultText as it is", otherwise the translation of
    //! m_key, or m_defaultText if there is none.
    struct SlotBinding
    {
        AZStd::string m_channelName; //!< engine name of the key, e.g. "keyboard_key_edit_space"
        AZStd::string m_key;         //!< localization key, e.g. "input.key.space"; empty for letters, digits, F-keys
        AZStd::string m_defaultText; //!< English text, e.g. "Space"
    };

    //! Another action that already uses a key.
    struct BindingConflict
    {
        AZStd::string m_actionName;
        BindingSlot m_slot = BindingSlot::Primary;
    };

    //! What SetBinding does with other actions that already use the same key in the same device group.
    enum class ConflictMode : AZ::u8
    {
        Move,           //!< the key is removed from them: it now works only for the new action
        AllowDuplicate, //!< they keep it: one key triggers several actions
    };

    enum class SetBindingResult : AZ::u8
    {
        Ok,
        UnknownAction,    //!< no active bindings have an action with this name
        UnknownChannel,   //!< not a keyboard, mouse or gamepad channel of the engine
        WrongDeviceGroup, //!< e.g. a gamepad button for a keyboard and mouse slot
        WrongChannelKind, //!< e.g. a key for an action that expects mouse movement
        ReservedChannel,  //!< Escape and gamepad Start: they cancel StartCapture and stay with the menus
    };

    //! Rebinding API for game code, e.g. a settings menu.
    //!
    //! The player's changes are stored as differences from the defaults in the .keybinds assets and apply
    //! to every entity that has the action. Changes take effect at once and send
    //! InputHintsContractNotifications::OnInputHintsChanged, but they are written to the player's file
    //! only by SaveBindings. The file is loaded when the gem starts.
    //! Implemented by RebindableInputSystemComponent; reachable through RebindableInputInterface and RebindableInputRequestBus.
    class RebindableInputRequests
    {
    public:
        AZ_RTTI(RebindableInputRequests, RebindableInputRequestsTypeId);
        virtual ~RebindableInputRequests() = default;

        //! Names of all input actions with active bindings, without duplicates, in the order of the .keybinds files.
        virtual AZStd::vector<AZStd::string> GetActions() const = 0;

        //! What is in a slot now (default or changed by the player), or an empty optional for an empty slot
        //! or an unknown action.
        virtual AZStd::optional<SlotBinding> GetBinding(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) const = 0;

        //! Another action (or the other slot of the same action) that already uses this key in this device group,
        //! or an empty optional. Ask before SetBinding to show "this key is already used by ...".
        virtual AZStd::optional<BindingConflict> FindConflict(
            const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName) const = 0;

        //! Puts a key into a slot. Only a channel of the same kind as the action's defaults is accepted:
        //! a button for a button, a mouse axis for a mouse axis, a stick for a stick.
        virtual SetBindingResult SetBinding(
            const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName, ConflictMode mode) = 0;

        //! Empties a slot. Any slot may be emptied, even both slots of an action.
        virtual void ClearBinding(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) = 0;

        //! Returns all slots of the action to the defaults.
        virtual void ResetAction(const AZStd::string& actionName) = 0;

        //! Returns all actions to the defaults.
        virtual void ResetAll() = 0;

        //! Writes the player's changes to the player's file, e.g. for "Apply" in a settings menu.
        //! Asynchronous: the result comes as RebindableInputNotifications::OnBindingsSaved, on the main thread,
        //! possibly already during this call. Where the file is depends on the setting "/RebindableInput/Storage"
        //! ("ProjectUser" or "SaveData"; by default "SaveData" in release builds and "ProjectUser" otherwise).
        virtual void SaveBindings() = 0;

        //! Returns to the player's changes of the last successful save or load, e.g. for "Cancel" in a settings menu.
        virtual void RevertBindings() = 0;

        //! Waits for the player's next key for a slot ("press a key" in a settings menu). Nothing is bound: the key is
        //! reported as RebindableInputNotifications::OnChannelCaptured, and the menu checks FindConflict and calls SetBinding.
        //! - Starts with the next frame, so the key that started it (e.g. Enter on a menu button) is not taken.
        //! - Only a channel of the slot's device group and of the kind the action expects is taken (a button for a button
        //!   action; analog channels past half way). Until then, all input is swallowed: above UI, below ImGui.
        //! - The taken key stays swallowed until it is released, so the new binding does not fire at once.
        //! - Escape or gamepad Start cancels (OnCaptureCancelled). A capture in progress is cancelled by a new one.
        //! Returns false for an unknown action or a slot that accepts no channel.
        virtual bool StartCapture(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) = 0;

        //! Stops waiting for a key and sends OnCaptureCancelled; nothing happens if no capture is in progress.
        virtual void CancelCapture() = 0;

        virtual bool IsCapturing() const = 0;

        //! The device group the player uses right now: switched by the player's input (not by mouse movement), and before
        //! the first input by a gamepad connecting. Input hints show the keys of this group.
        //! Changes are sent as RebindableInputNotifications::OnActiveDeviceGroupChanged.
        virtual DeviceGroup GetActiveDeviceGroup() const = 0;
    };

    class RebindableInputBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using RebindableInputRequestBus = AZ::EBus<RebindableInputRequests, RebindableInputBusTraits>;
    using RebindableInputInterface = AZ::Interface<RebindableInputRequests>;

} // namespace RebindableInput
