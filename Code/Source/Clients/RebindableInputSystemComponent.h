
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Asset/GenericAssetHandler.h>
#include <InputHintsContract/InputHintsContractBus.h>
#include <RebindableInput/RebindableInputBus.h>

#include <BindingsRegistry.h>
#include <InputEventBindings.h>

namespace RebindableInput
{
    //! Reflects the binding classes, registers the .keybinds asset type (like StartingPointInputSystemComponent
    //! does for .inputbindings) and owns the active input bindings of every entity with a RebindableInputComponent.
    //! Implements InputHintsContract: tells UI which key is bound to an input action.
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

        //! The active bindings of one entity.
        struct EntityBindings
        {
            AZ::EntityId m_entityId;
            //! A clone of the asset's bindings. Owns raw pointers to its input handlers:
            //! release them with ReleaseBindings, never copy this object.
            InputEventBindings m_bindings;
            //! The local user the bindings are activated for; Deactivate must get the same value.
            AzFramework::LocalUserId m_localUserId = AzFramework::LocalUserIdNone;
        };

        //! Returns the entry of the entity, or nullptr if the entity has no bindings.
        EntityBindings* FindEntityBindings(AZ::EntityId entityId);

        //! Deactivates the entry's bindings and frees its input handlers. The entry stays in the list, empty.
        static void ReleaseBindings(EntityBindings& entry);

        //! Warns about actions of the entry that are also bound on another entity:
        //! input hints show the bindings of the entity registered first.
        void WarnAboutDuplicateActions(const EntityBindings& entry) const;

        //! Tells UI to request its input hints again. Call at the very end of a change, when the state is complete.
        static void NotifyInputHintsChanged();

        //! Loads and saves .keybinds files. Requires the asset manager ("AssetDatabaseService").
        AZStd::unique_ptr<AzFramework::GenericAssetHandler<InputEventBindingsAsset>> m_inputEventBindingsAssetHandler;

        //! Active bindings in registration order, so lookups over all entities are deterministic.
        //! Stored by pointer: the bindings never move in memory when the vector grows or an entry is erased.
        AZStd::vector<AZStd::unique_ptr<EntityBindings>> m_entityBindings;
    };

} // namespace RebindableInput
