
#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzFramework/Input/User/LocalUserId.h>

#include <InputEventBindings.h>

namespace RebindableInput
{
    //! Internal interface between RebindableInputComponent and RebindableInputSystemComponent.
    //!
    //! The system component owns the active input bindings of every entity: it clones them from the asset,
    //! activates and deactivates them. A RebindableInputComponent only holds its asset and local user id
    //! and reports to the system component through this interface.
    //!
    //! This header is in Source/, so it is private to the gem: other gems and game code cannot call it.
    //! Bindings are keyed by entity id: there is at most one RebindableInputComponent per entity
    //! (it is incompatible with "InputConfigurationService").
    //!
    //! Implemented and registered by RebindableInputSystemComponent. Reached only through
    //! BindingsRegistryInterface (no bus: there is exactly one caller and one implementation).
    class BindingsRegistryRequests
    {
    public:
        AZ_RTTI(BindingsRegistryRequests, "{19D43747-83B7-4037-BB11-05BA2D0A0251}");
        virtual ~BindingsRegistryRequests() = default;

        //! Called by the component when its asset is ready (OnAssetReady) or was reloaded (OnAssetReloaded).
        //! Replaces the entity's bindings, if any, with a fresh clone of the asset's bindings and activates them
        //! for the given local user. The asset is only read during the call; the component keeps holding it.
        virtual void SetBindings(
            AZ::EntityId entityId,
            const AZ::Data::Asset<InputEventBindingsAsset>& asset,
            AzFramework::LocalUserId localUserId) = 0;

        //! Called by the component when it is deactivated.
        //! Deactivates and forgets the entity's bindings. Does nothing for an unknown entity:
        //! the component may be deactivated before its asset was ever ready.
        virtual void RemoveBindings(AZ::EntityId entityId) = 0;

        //! Called by the component when its local user id changes (InputConfigurationComponentRequests::SetLocalUserId).
        //! Deactivates the entity's bindings for the old user and activates them for the new one.
        virtual void SetLocalUserId(AZ::EntityId entityId, AzFramework::LocalUserId localUserId) = 0;
    };

    using BindingsRegistryInterface = AZ::Interface<BindingsRegistryRequests>;

} // namespace RebindableInput
