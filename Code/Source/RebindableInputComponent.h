/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * Forked from Gems/StartingPointInput/Code/Source/InputConfigurationComponent.h (O3DE, tag 2605.0)
 * into the RebindableInput gem.
 * Changes: renamed to RebindableInputComponent, moved to the RebindableInput namespace, new type id,
 * no serialization version history (this is a new component).
 *
 */
#pragma once

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzFramework/Components/EditorEntityEvents.h>

#include <StartingPointInput/InputEventRequestBus.h>

#include <InputEventBindings.h>

namespace AZ
{
    class SerializeContext;
}

namespace RebindableInput
{
    //! Binds raw input to named input events, like StartingPointInput's Input component,
    //! but loads the bindings from a .keybinds asset of this gem.
    //!
    //! The events are sent on StartingPointInput::InputEventNotificationBus, so listeners do not change.
    //! The component provides "InputConfigurationService", like the original, so components that require
    //! that service keep working. It is incompatible with that service too, so it cannot be placed on the same
    //! entity as StartingPointInput's Input component (that would send every event twice).
    class RebindableInputComponent
        : public AZ::Component
        , private AZ::Data::AssetBus::Handler
        , private StartingPointInput::InputConfigurationComponentRequestBus::Handler
        , public AzFramework::EditorEntityEvents
    {
    public:
        AZ_COMPONENT(RebindableInputComponent, "{5C5BB01D-59E7-4A08-A8C9-1B3C6CF712A5}", AzFramework::EditorEntityEvents);
        RebindableInputComponent();
        ~RebindableInputComponent() override;
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::EditorEntityEvents
        void EditorSetPrimaryAsset(const AZ::Data::AssetId& assetId) override;
        //////////////////////////////////////////////////////////////////////////

    private:
        RebindableInputComponent(const RebindableInputComponent&) = delete;

        //////////////////////////////////////////////////////////////////////////
        // StartingPointInput::InputConfigurationComponentRequestBus::Handler
        void SetLocalUserId(AzFramework::LocalUserId localUserId) override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // AZ::Data::AssetBus::Handler
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        //////////////////////////////////////////////////////////////////////////

        void ActivateBindingsIfAppropriate();

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        AZ::Data::Asset<InputEventBindingsAsset> m_inputEventBindingsAsset;
        AZ::s32 m_localPlayerIndex = -1;

        //////////////////////////////////////////////////////////////////////////
        // Non Reflected Data
        // A working copy of the bindings from the asset (cloned in OnAssetReady).
        InputEventBindings m_inputEventBindings;
        AzFramework::LocalUserId m_localUserId = AzFramework::LocalUserIdAny;

        // Unlike the definition of most assets, the input asset requires additional preparation after its loaded
        // in order to actually be prepared to be used.
        bool m_isAssetPrepared = false;

        AZ::Uuid m_uuid;
    };
} // namespace RebindableInput
