
#include "RebindableInputSystemComponent.h"

#include <RebindableInput/RebindableInputTypeIds.h>

#include <InputEventGroup.h>
#include <InputEventMap.h>
#include <RebindableInputComponent.h>
#include <BindingsRegistry.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <InputHintsContract/InputHintsContractNotificationBus.h>

namespace RebindableInput
{
    AZ_COMPONENT_IMPL(RebindableInputSystemComponent, "RebindableInputSystemComponent",
        RebindableInputSystemComponentTypeId);

    void RebindableInputSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        // The binding classes are not components, so nothing else reflects them.
        // Same list as StartingPointInputSystemComponent::Reflect.
        InputEventBindingsAsset::Reflect(context);
        InputEventBindings::Reflect(context);
        InputEventGroup::Reflect(context);
        InputEventMap::Reflect(context);
        ThumbstickInputEventMap::Reflect(context);

        // StartingPointInputSystemComponent::Reflect also reflects InputEventNotificationId and
        // InputEventNotificationBus for scripts. That is not repeated here: both belong to StartingPointInput,
        // which is enabled as a dependency and reflects them already.

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<RebindableInputSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void RebindableInputSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("RebindableInputService"));
    }

    void RebindableInputSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("RebindableInputService"));
    }

    void RebindableInputSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The asset handler is registered in Activate and needs the asset manager to be ready.
        required.push_back(AZ_CRC_CE("AssetDatabaseService"));
        required.push_back(AZ_CRC_CE("AssetCatalogService"));
    }

    void RebindableInputSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    RebindableInputSystemComponent::RebindableInputSystemComponent()
    {
        if (RebindableInputInterface::Get() == nullptr)
        {
            RebindableInputInterface::Register(this);
        }
    }

    RebindableInputSystemComponent::~RebindableInputSystemComponent()
    {
        if (RebindableInputInterface::Get() == this)
        {
            RebindableInputInterface::Unregister(this);
        }
    }

    void RebindableInputSystemComponent::Init()
    {
    }

    void RebindableInputSystemComponent::Activate()
    {
        AZ_Assert(AZ::Data::AssetManager::IsReady(), "Asset manager isn't ready!");

        // Same as StartingPointInputSystemComponent, with the "keybinds" extension instead of "inputbindings".
        // The last argument is the component that uses this asset: the Editor offers it when the asset is dropped on an entity.
        m_inputEventBindingsAssetHandler = AZStd::make_unique<AzFramework::GenericAssetHandler<InputEventBindingsAsset>>(
            "Rebindable Input Bindings", "Other", "keybinds", AZ::AzTypeInfo<RebindableInputComponent>::Uuid());
        m_inputEventBindingsAssetHandler->Register();

        RebindableInputRequestBus::Handler::BusConnect();

        // Last: components can report their bindings only once everything above is ready.
        const auto outcome = BindingsRegistryInterface::Register(this);
        AZ_Error("RebindableInput", outcome.IsSuccess(),
            "Failed to register the bindings registry: %s", outcome.GetError().c_str());

        // InputHintsContract requires both access paths.
        InputHintsContract::InputHintsContractRequestBus::Handler::BusConnect();
        const auto hintsOutcome = InputHintsContract::InputHintsContractInterface::Register(this);
        AZ_Error("RebindableInput", hintsOutcome.IsSuccess(),
            "Failed to register as the InputHintsContract implementation: %s", hintsOutcome.GetError().c_str());
    }

    void RebindableInputSystemComponent::Deactivate()
    {
        // First: nobody can ask for hints or add bindings while the rest is being shut down.
        if (InputHintsContract::InputHintsContractInterface::Get() == this)
        {
            InputHintsContract::InputHintsContractInterface::Unregister(this);
        }
        InputHintsContract::InputHintsContractRequestBus::Handler::BusDisconnect();

        if (BindingsRegistryInterface::Get() == this)
        {
            BindingsRegistryInterface::Unregister(this);
        }

        // Bindings still registered here belong to entities that were not deactivated before this component.
        for (AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            ReleaseBindings(*entry);
        }
        m_entityBindings.clear();

        RebindableInputRequestBus::Handler::BusDisconnect();

        // Unregistered from the asset manager by AZ::Data::AssetHandler's destructor.
        m_inputEventBindingsAssetHandler.reset();
    }

    void RebindableInputSystemComponent::SetBindings(
        AZ::EntityId entityId,
        const AZ::Data::Asset<InputEventBindingsAsset>& asset,
        AzFramework::LocalUserId localUserId)
    {
        const InputEventBindingsAsset* inputAsset = asset.Get();
        if (!inputAsset)
        {
            AZ_Error("RebindableInput", false, "Entity %s: the input bindings asset '%s' is not loaded.",
                entityId.ToString().c_str(), asset.GetHint().c_str());
            return;
        }

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
        if (!serializeContext)
        {
            AZ_Error("RebindableInput", false, "Entity %s: no serialize context, the input bindings cannot be cloned.",
                entityId.ToString().c_str());
            return;
        }

        EntityBindings* entry = FindEntityBindings(entityId);
        if (entry)
        {
            // The asset was reloaded: drop the old clone, but keep the entry at its place in the registration order.
            ReleaseBindings(*entry);
        }
        else
        {
            entry = m_entityBindings.emplace_back(AZStd::make_unique<EntityBindings>()).get();
            entry->m_entityId = entityId;
        }

        // The asset is shared and read-only, so every entity works on its own clone.
        // Swap instead of assignment: assigning would copy the raw pointers to the input handlers.
        InputEventBindings freshBindings;
        serializeContext->CloneObjectInplace<InputEventBindings>(freshBindings, &inputAsset->m_bindings);
        entry->m_bindings.Swap(&freshBindings);

        entry->m_localUserId = localUserId;
        if (localUserId != AzFramework::LocalUserIdNone)
        {
            entry->m_bindings.Activate(localUserId);
        }

        WarnAboutDuplicateActions(*entry);
        NotifyInputHintsChanged();
    }

    void RebindableInputSystemComponent::RemoveBindings(AZ::EntityId entityId)
    {
        for (auto it = m_entityBindings.begin(); it != m_entityBindings.end(); ++it)
        {
            if ((*it)->m_entityId == entityId)
            {
                // Release before erase: erase only destroys the entry, it does not free the input handlers.
                ReleaseBindings(**it);
                m_entityBindings.erase(it);
                NotifyInputHintsChanged();
                return;
            }
        }
        // Not found is fine: the component may be deactivated before its asset was ever ready.
    }

    void RebindableInputSystemComponent::SetLocalUserId(AZ::EntityId entityId, AzFramework::LocalUserId localUserId)
    {
        EntityBindings* entry = FindEntityBindings(entityId);
        if (!entry || entry->m_localUserId == localUserId)
        {
            // No bindings yet: the component passes its current local user id with SetBindings later.
            return;
        }

        // No NotifyInputHintsChanged here: hints do not depend on the local user (yet).

        if (entry->m_localUserId != AzFramework::LocalUserIdNone)
        {
            entry->m_bindings.Deactivate(entry->m_localUserId);
        }
        entry->m_localUserId = localUserId;
        if (entry->m_localUserId != AzFramework::LocalUserIdNone)
        {
            entry->m_bindings.Activate(entry->m_localUserId);
        }
    }

    RebindableInputSystemComponent::EntityBindings* RebindableInputSystemComponent::FindEntityBindings(AZ::EntityId entityId)
    {
        for (AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            if (entry->m_entityId == entityId)
            {
                return entry.get();
            }
        }
        return nullptr;
    }

    AZStd::optional<InputHintsContract::InputHint> RebindableInputSystemComponent::GetInputHint(const AZStd::string& actionName) const
    {
        // The first keyboard or mouse binding of the action, searching entities in registration order.
        // TODO: pick by the device group the player uses right now (keyboard and mouse or gamepad).
        // TODO: turn the channel name (e.g. "keyboard_key_alphanumeric_E") into display text (e.g. "E").
        for (const AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            for (const InputEventGroup& group : entry->m_bindings.m_inputEventGroups)
            {
                if (group.m_eventName != actionName)
                {
                    continue;
                }

                for (const InputSubComponent* handler : group.m_inputHandlers)
                {
                    // Only InputEventMap (and ThumbstickInputEventMap, which is filtered out by its gamepad device) has a channel.
                    const InputEventMap* inputEventMap = azrtti_cast<const InputEventMap*>(handler);
                    if (!inputEventMap)
                    {
                        continue;
                    }

                    if (inputEventMap->m_inputDeviceType == AzFramework::InputDeviceKeyboard::Id.GetName() ||
                        inputEventMap->m_inputDeviceType == AzFramework::InputDeviceMouse::Id.GetName())
                    {
                        InputHintsContract::InputHint hint;
                        hint.m_text = inputEventMap->m_inputName;
                        return hint;
                    }
                }
            }
        }
        return AZStd::nullopt;
    }

    void RebindableInputSystemComponent::WarnAboutDuplicateActions([[maybe_unused]] const EntityBindings& entry) const
    {
    #if defined(AZ_ENABLE_TRACING)
        for (const InputEventGroup& group : entry.m_bindings.m_inputEventGroups)
        {
            for (const AZStd::unique_ptr<EntityBindings>& other : m_entityBindings)
            {
                if (other.get() == &entry)
                {
                    continue;
                }
                for (const InputEventGroup& otherGroup : other->m_bindings.m_inputEventGroups)
                {
                    AZ_Warning("RebindableInput", otherGroup.m_eventName != group.m_eventName,
                        "Input action '%s' is bound on entity %s and on entity %s. "
                        "Input hints show the bindings of the entity that registered first.",
                        group.m_eventName.c_str(), other->m_entityId.ToString().c_str(), entry.m_entityId.ToString().c_str());
                }
            }
        }
    #endif // AZ_ENABLE_TRACING
    }

    void RebindableInputSystemComponent::NotifyInputHintsChanged()
    {
        InputHintsContract::InputHintsContractNotificationBus::Broadcast(
            &InputHintsContract::InputHintsContractNotifications::OnInputHintsChanged);
    }

    void RebindableInputSystemComponent::ReleaseBindings(EntityBindings& entry)
    {
        if (entry.m_localUserId != AzFramework::LocalUserIdNone)
        {
            entry.m_bindings.Deactivate(entry.m_localUserId);
        }
        entry.m_bindings.Cleanup();
        entry.m_localUserId = AzFramework::LocalUserIdNone;
    }

} // namespace RebindableInput
