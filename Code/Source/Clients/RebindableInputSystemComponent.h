
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Asset/GenericAssetHandler.h>
#include <InputHintsContract/InputHintsContractBus.h>
#include <RebindableInput/RebindableInputBus.h>

#include <BindingsRegistry.h>
#include <InputChannelInfo.h>
#include <InputChannelText.h>
#include <InputEventBindings.h>
#include <PlayerBindingsFile.h>
#include <PlayerBindingsStorage.h>

namespace AZ
{
    class SerializeContext;
}

namespace RebindableInput
{
    class ActiveDeviceTracker;
    class InputCapture;
    class InputEventMap;
    class RebindableInputConsoleCommands;

    //! Reflects the binding classes, registers the .keybinds asset type (like StartingPointInputSystemComponent
    //! does for .inputbindings) and owns the active input bindings of every entity with a RebindableInputComponent.
    //! Implements InputHintsContract (which key to show for an action) and the rebinding API (RebindableInputRequests).
    //!
    //! Every entity's bindings are built the same way, on registration and after every change of the player:
    //! clone the asset's bindings -> sort them into key slots -> apply the player's changes -> activate.
    //! The player's changes are loaded from the player's file in Activate and written by SaveBindings.
    class RebindableInputSystemComponent
        : public AZ::Component
        , protected RebindableInputRequestBus::Handler
        , private BindingsRegistryRequests
        , private InputHintsContract::InputHintsContractRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(RebindableInputSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        RebindableInputSystemComponent();
        ~RebindableInputSystemComponent();

    protected:
        ////////////////////////////////////////////////////////////////////////
        // RebindableInputRequestBus interface implementation
        AZStd::vector<AZStd::string> GetActions() const override;
        AZStd::optional<SlotBinding> GetBinding(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) const override;
        AZStd::optional<BindingConflict> FindConflict(
            const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName) const override;
        SetBindingResult SetBinding(
            const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName, ConflictMode mode) override;
        void ClearBinding(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) override;
        void ResetAction(const AZStd::string& actionName) override;
        void ResetAll() override;
        void SaveBindings() override;
        void RevertBindings() override;
        bool StartCapture(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) override;
        void CancelCapture() override;
        bool IsCapturing() const override;
        DeviceGroup GetActiveDeviceGroup() const override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        ////////////////////////////////////////////////////////////////////////
        // BindingsRegistryRequests
        void SetBindings(
            AZ::EntityId entityId,
            const AZ::Data::Asset<InputEventBindingsAsset>& asset,
            AzFramework::LocalUserId localUserId) override;
        void RemoveBindings(AZ::EntityId entityId) override;
        void SetLocalUserId(AZ::EntityId entityId, AzFramework::LocalUserId localUserId) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // InputHintsContract::InputHintsContractRequests
        AZStd::optional<InputHintsContract::InputHint> GetInputHint(const AZStd::string& actionName) const override;
        ////////////////////////////////////////////////////////////////////////

        //! Channel names per device group and slot; an empty name is an empty slot.
        using SlotChannels = AZStd::array<AZStd::array<AZStd::string, BindingSlotCount>, DeviceGroupCount>;

        //! The key slots of one action of one entity.
        struct ActionSlots
        {
            AZStd::string m_actionName;
            SlotChannels m_defaultChannels; //!< from the asset (the first two bindings of each device group)
            SlotChannels m_channels;        //!< what is active now: the defaults with the player's changes applied
        };

        //! The active bindings of one entity.
        struct EntityBindings
        {
            AZ::EntityId m_entityId;
            //! Kept to rebuild the bindings after the player changes a key.
            AZ::Data::Asset<InputEventBindingsAsset> m_asset;
            //! A clone of the asset's bindings, sorted into slots. Owns raw pointers to its input handlers:
            //! release them with ReleaseBindings, never copy this object.
            InputEventBindings m_bindings;
            //! The key slots of every action, for the rebinding API and the input hints.
            AZStd::vector<ActionSlots> m_actions;
            //! The local user the bindings are activated for; Deactivate must get the same value.
            AzFramework::LocalUserId m_localUserId = AzFramework::LocalUserIdNone;
        };

        //! Returns the entry of the entity, or nullptr if the entity has no bindings.
        EntityBindings* FindEntityBindings(AZ::EntityId entityId);

        //! The slots of the action on the first entity (in registration order) that has it, or nullptr.
        const ActionSlots* FindActionSlots(const AZStd::string& actionName) const;

        //! Clones the entry's asset, sorts the bindings into slots, applies the player's changes and activates them.
        //! registering: the defaults may be new (the entity registers, its asset was reloaded, the player's file arrived):
        //! report bindings that do not fit, resolve defaults that took a key of the player, and remember the defaults.
        //! False after the player's own changes, where the defaults are already known.
        void BuildBindings(EntityBindings& entry, bool registering);

        //! Sorts the handlers of one group (action) into slots and makes them match the slots after the player's changes:
        //! changes the channel of existing handlers, creates handlers for filled empty slots, frees handlers of emptied slots.
        //! Defaults emptied because the player took their key are added to resolvedConflicts (only when registering).
        void ApplySlots(InputEventGroup& group, ActionSlots& action, AZ::SerializeContext& serializeContext, AZ::EntityId entityId,
            bool registering, AZStd::vector<SlotRecord>& resolvedConflicts) const;

        //! Rebuilds the bindings of every entity: after the player's changes, or with registering after the player's file arrived.
        void RebuildAllBindings(bool registering = false);

        //! The text of a key for the player: the key and English text from InputChannelText, and for letters and punctuation
        //! the character printed on the key in the player's keyboard layout (asked from the engine every time: the player
        //! may switch the layout while playing). A channel without text gives its raw name.
        static KeyText GetKeyText(const AZStd::string& channelName);

        //! The kind of channel a slot of the action accepts: the kind of its defaults in this device group.
        //! Without defaults there, a button action in the other group accepts buttons; an axis action accepts nothing
        //! (it cannot be moved to another device).
        static AZStd::optional<ChannelKind> GetExpectedKind(const ActionSlots& action, DeviceGroup group);

        //! Whether the channel fits a slot of the action in this device group (device group and kind, not a reserved key).
        static bool AcceptsChannel(const ActionSlots& action, DeviceGroup group, const AZStd::string& channelName);

        //! Whether one of the player's changes puts this channel into a slot of this device group.
        bool IsTakenByPlayer(DeviceGroup group, const AZStd::string& channelName) const;

        //! The default of the slot as the player last saw it, or nullptr for a slot the player has not seen.
        const AZStd::string* FindKnownDefault(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) const;

        //! Remembers the current defaults of the action: they are written with the next SaveBindings.
        void RememberDefaults(const ActionSlots& action);

        //! Asks the storage for the player's file; the answer may come later (SaveData).
        void LoadPlayerBindings();

        //! Applies the player's file: replaces the player's changes and the known defaults and rebuilds the bindings.
        void OnPlayerBindingsLoaded(PlayerBindingsStorage::LoadResult result, const AZStd::string& text);

        //! Sends RebindableInputNotifications::OnBindingsSaved.
        static void NotifyBindingsSaved(bool success);

        //! Stores a change of the player; a change back to the default removes the stored change.
        void SetOverride(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName);

        //! Deactivates the entry's bindings and frees its input handlers. The entry stays in the list, empty.
        static void ReleaseBindings(EntityBindings& entry);

        //! Frees one input handler the way InputEventGroup::Cleanup does (it was allocated by the serializer).
        static void FreeHandler(InputSubComponent* handler);

        //! Warns about actions of the entry that are also bound on another entity:
        //! input hints show the bindings of the entity registered first.
        void WarnAboutDuplicateActions(const EntityBindings& entry) const;

        //! Warns about channels of the entry that InputChannelText has no text for:
        //! their input hints show the raw channel name.
        static void WarnAboutChannelsWithoutText(const EntityBindings& entry);

        //! Tells UI to request its input hints again. Call at the very end of a change, when the state is complete.
        static void NotifyInputHintsChanged();

        //! Loads and saves .keybinds files. Requires the asset manager ("AssetDatabaseService").
        AZStd::unique_ptr<AzFramework::GenericAssetHandler<InputEventBindingsAsset>> m_inputEventBindingsAssetHandler;

        //! Active bindings in registration order, so lookups over all entities are deterministic.
        //! Stored by pointer: the bindings never move in memory when the vector grows or an entry is erased.
        AZStd::vector<AZStd::unique_ptr<EntityBindings>> m_entityBindings;

        //! The player's changes (slots that differ from the default; an empty channel is an emptied slot),
        //! applied to every entity that has the action.
        AZStd::vector<SlotRecord> m_overrides;

        //! The player's changes as of the last successful save or load: RevertBindings returns to them.
        AZStd::vector<SlotRecord> m_savedOverrides;

        //! The defaults of every slot the player has seen: from the file, updated when entities register.
        //! A default that differs from it is new to the player (see BuildBindings).
        AZStd::vector<SlotRecord> m_knownDefaults;

        //! Exists while the component is active. Storage callbacks hold a weak_ptr to it:
        //! an answer that comes after Deactivate is ignored instead of touching a deactivated component.
        AZStd::shared_ptr<int> m_callbackGuard;

        //! The player's file was asked for and has not arrived yet: SaveBindings would overwrite it.
        bool m_loadingPlayerBindings = false;

        //! Which device group the player uses; exists while the component is active.
        AZStd::unique_ptr<ActiveDeviceTracker> m_activeDeviceTracker;

        //! "Press a key" for StartCapture; exists while the component is active.
        AZStd::unique_ptr<InputCapture> m_inputCapture;

        //! Debug console commands (rebind_list, rebind_set, ...); registered while this exists, only in builds with tracing (not in release).
        AZStd::unique_ptr<RebindableInputConsoleCommands> m_consoleCommands;
    };

} // namespace RebindableInput
