
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Asset/GenericAssetHandler.h>
#include <InputHintsContract/InputHintsContractBus.h>
#include <RebindableInput/RebindableInputBus.h>

#include <BindingsRegistry.h>
#include <InputEventBindings.h>

namespace AZ
{
    class SerializeContext;
}

namespace RebindableInput
{
    class InputEventMap;
    class RebindableInputConsoleCommands;

    //! Reflects the binding classes, registers the .keybinds asset type (like StartingPointInputSystemComponent
    //! does for .inputbindings) and owns the active input bindings of every entity with a RebindableInputComponent.
    //! Implements InputHintsContract (which key to show for an action) and the rebinding API (RebindableInputRequests).
    //!
    //! Every entity's bindings are built the same way, on registration and after every change of the player:
    //! clone the asset's bindings -> sort them into key slots -> apply the player's changes -> activate.
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

        //! One change of the player: a slot that differs from the default.
        struct PlayerOverride
        {
            AZStd::string m_actionName;
            DeviceGroup m_group = DeviceGroup::KeyboardMouse;
            BindingSlot m_slot = BindingSlot::Primary;
            AZStd::string m_channelName; //!< empty: the player emptied the slot
        };

        //! Returns the entry of the entity, or nullptr if the entity has no bindings.
        EntityBindings* FindEntityBindings(AZ::EntityId entityId);

        //! The slots of the action on the first entity (in registration order) that has it, or nullptr.
        const ActionSlots* FindActionSlots(const AZStd::string& actionName) const;

        //! Clones the entry's asset, sorts the bindings into slots, applies the player's changes and activates them.
        //! warn: report bindings that do not fit into the slots (only on registration, not on every change).
        void BuildBindings(EntityBindings& entry, bool warn);

        //! Sorts the handlers of one group (action) into slots and makes them match the slots after the player's changes:
        //! changes the channel of existing handlers, creates handlers for filled empty slots, frees handlers of emptied slots.
        void ApplySlots(InputEventGroup& group, ActionSlots& action, AZ::SerializeContext& serializeContext, AZ::EntityId entityId, bool warn) const;

        //! Rebuilds the bindings of every entity: called after the player's changes.
        void RebuildAllBindings();

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

        //! The player's changes, applied to every entity that has the action. Saved and loaded in a later step.
        AZStd::vector<PlayerOverride> m_overrides;

        //! Debug console commands (rebind_list, rebind_set, ...); registered while this exists, only in builds with tracing.
        AZStd::unique_ptr<RebindableInputConsoleCommands> m_consoleCommands;
    };

} // namespace RebindableInput
