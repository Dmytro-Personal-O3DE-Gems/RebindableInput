
#include "RebindableInputSystemComponent.h"

#include <RebindableInput/RebindableInputTypeIds.h>

#include <InputChannelInfo.h>
#include <InputChannelText.h>
#include <InputEventGroup.h>
#include <InputEventMap.h>
#include <RebindableInputComponent.h>
#include <RebindableInputConsoleCommands.h>
#include <BindingsRegistry.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
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

#if defined(AZ_ENABLE_TRACING)
        // After the rebinding API is ready: the commands only call it.
        // Only with tracing: the commands print through AZ_Printf, which release builds compile out.
        m_consoleCommands = AZStd::make_unique<RebindableInputConsoleCommands>();
#endif
    }

    void RebindableInputSystemComponent::Deactivate()
    {
        // Unregistered from the console by ConsoleFunctorBase's destructor.
        m_consoleCommands.reset();

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
        if (!asset.Get())
        {
            AZ_Error("RebindableInput", false, "Entity %s: the input bindings asset '%s' is not loaded.",
                entityId.ToString().c_str(), asset.GetHint().c_str());
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

        entry->m_asset = asset;
        entry->m_localUserId = localUserId;
        BuildBindings(*entry, true);

        WarnAboutDuplicateActions(*entry);
        WarnAboutChannelsWithoutText(*entry);
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

    ////////////////////////////////////////////////////////////////////////
    // Building the bindings

    void RebindableInputSystemComponent::BuildBindings(EntityBindings& entry, bool warn)
    {
        const InputEventBindingsAsset* inputAsset = entry.m_asset.Get();
        if (!inputAsset)
        {
            return; // SetBindings already reported it
        }

        AZ::SerializeContext* serializeContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
        if (!serializeContext)
        {
            AZ_Error("RebindableInput", false, "Entity %s: no serialize context, the input bindings cannot be cloned.",
                entry.m_entityId.ToString().c_str());
            return;
        }

        // The asset is shared and read-only, so every entity works on its own clone.
        // Swap instead of assignment: assigning would copy the raw pointers to the input handlers.
        InputEventBindings freshBindings;
        serializeContext->CloneObjectInplace<InputEventBindings>(freshBindings, &inputAsset->m_bindings);
        entry.m_bindings.Swap(&freshBindings);

        // Slots and the player's changes are applied to the clone before it is activated.
        entry.m_actions.clear();
        for (InputEventGroup& group : entry.m_bindings.m_inputEventGroups)
        {
            ActionSlots& action = entry.m_actions.emplace_back();
            action.m_actionName = group.m_eventName;
            ApplySlots(group, action, *serializeContext, entry.m_entityId, warn);
        }

        if (entry.m_localUserId != AzFramework::LocalUserIdNone)
        {
            entry.m_bindings.Activate(entry.m_localUserId);
        }
    }

    void RebindableInputSystemComponent::ApplySlots(
        InputEventGroup& group, ActionSlots& action, AZ::SerializeContext& serializeContext,
        [[maybe_unused]] AZ::EntityId entityId, [[maybe_unused]] bool warn) const
    {
        // 1. Sort the handlers into slots: the first two of each device group, in file order.
        //    Handlers of other devices (or of unknown types) are not slot-managed and stay as they are.
        AZStd::array<AZStd::array<InputEventMap*, BindingSlotCount>, DeviceGroupCount> slots{};
        AZStd::array<size_t, DeviceGroupCount> handlerCounts{};
        AZStd::vector<InputSubComponent*> otherHandlers;
        for (InputSubComponent* handler : group.m_inputHandlers)
        {
            InputEventMap* inputEventMap = azrtti_cast<InputEventMap*>(handler);
            const AZStd::optional<DeviceGroup> deviceGroup =
                inputEventMap ? InputChannelInfo::GetDeviceGroup(inputEventMap->m_inputDeviceType) : AZStd::nullopt;
            if (!deviceGroup)
            {
                otherHandlers.push_back(handler);
                continue;
            }

            const size_t g = static_cast<size_t>(*deviceGroup);
            const size_t index = handlerCounts[g]++;
            if (index < BindingSlotCount)
            {
                slots[g][index] = inputEventMap;
                action.m_defaultChannels[g][index] = inputEventMap->m_inputName;
            }
            else
            {
                // Only a primary and an alternate key: a third binding could not be seen or changed in a settings menu.
                AZ_Warning("RebindableInput", !warn,
                    "Entity %s, input action '%s': more than %zu bindings for one device group, '%s' is disabled. "
                    "Only the first two (primary and alternate) are used.",
                    entityId.ToString().c_str(), group.m_eventName.c_str(), BindingSlotCount, inputEventMap->m_inputName.c_str());
                FreeHandler(handler);
            }
        }

        // 2. The player's changes on top of the defaults.
        action.m_channels = action.m_defaultChannels;
        for (const PlayerOverride& change : m_overrides)
        {
            if (change.m_actionName == action.m_actionName)
            {
                action.m_channels[static_cast<size_t>(change.m_group)][static_cast<size_t>(change.m_slot)] = change.m_channelName;
            }
        }

        // 3. Make the handlers match the slots.
        for (size_t g = 0; g < DeviceGroupCount; ++g)
        {
            for (size_t s = 0; s < BindingSlotCount; ++s)
            {
                const AZStd::string& channel = action.m_channels[g][s];
                InputEventMap*& handler = slots[g][s];

                if (channel.empty())
                {
                    // An emptied slot (or a slot without a default): no handler.
                    if (handler)
                    {
                        FreeHandler(handler);
                        handler = nullptr;
                    }
                    continue;
                }

                if (handler && handler->m_inputName == channel)
                {
                    continue; // the default, unchanged
                }

                if (!handler)
                {
                    // A filled empty slot needs a new handler. It is cloned from a handler of the same action,
                    // so it keeps the action's multiplier and dead zone (and the class, e.g. ThumbstickInputEventMap).
                    // SetBinding only allows channels of the same kind, so the template always fits.
                    const InputEventMap* sample = nullptr;
                    for (size_t sampleGroup : { g, (g + 1) % DeviceGroupCount })
                    {
                        for (const InputEventMap* candidate : slots[sampleGroup])
                        {
                            if (candidate && !sample)
                            {
                                sample = candidate;
                            }
                        }
                    }

                    // CloneObject allocates through the serializer's factory, like the handlers loaded from the asset,
                    // so InputEventGroup::Cleanup frees it correctly.
                    if (sample)
                    {
                        handler = serializeContext.CloneObject<InputEventMap>(sample);
                    }
                    else
                    {
                        const InputEventMap blank;
                        handler = serializeContext.CloneObject<InputEventMap>(&blank);
                    }
                    if (!handler)
                    {
                        continue;
                    }
                }

                handler->m_inputName = channel;
                if (const AZStd::optional<ChannelInfo> info = InputChannelInfo::Find(channel))
                {
                    handler->m_inputDeviceType = info->m_deviceType;
                }
            }
        }

        // 4. Put the handlers back: primary and alternate of each device group, then the rest.
        group.m_inputHandlers.clear();
        for (const auto& groupSlots : slots)
        {
            for (InputEventMap* handler : groupSlots)
            {
                if (handler)
                {
                    group.m_inputHandlers.push_back(handler);
                }
            }
        }
        group.m_inputHandlers.insert(group.m_inputHandlers.end(), otherHandlers.begin(), otherHandlers.end());
    }

    void RebindableInputSystemComponent::RebuildAllBindings()
    {
        for (AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            ReleaseBindings(*entry);
            BuildBindings(*entry, false);
        }
    }

    void RebindableInputSystemComponent::SetOverride(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName)
    {
        AZStd::erase_if(m_overrides, [&](const PlayerOverride& change)
        {
            return change.m_actionName == actionName && change.m_group == group && change.m_slot == slot;
        });

        // Only differences from the default are stored: a default added in a game update then reaches the player.
        const ActionSlots* action = FindActionSlots(actionName);
        const AZStd::string& defaultChannel =
            action ? action->m_defaultChannels[static_cast<size_t>(group)][static_cast<size_t>(slot)] : AZStd::string();
        if (channelName != defaultChannel)
        {
            m_overrides.push_back({ actionName, group, slot, channelName });
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // Rebinding API

    AZStd::vector<AZStd::string> RebindableInputSystemComponent::GetActions() const
    {
        AZStd::vector<AZStd::string> actionNames;
        for (const AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            for (const ActionSlots& action : entry->m_actions)
            {
                if (AZStd::find(actionNames.begin(), actionNames.end(), action.m_actionName) == actionNames.end())
                {
                    actionNames.push_back(action.m_actionName);
                }
            }
        }
        return actionNames;
    }

    AZStd::optional<SlotBinding> RebindableInputSystemComponent::GetBinding(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) const
    {
        const ActionSlots* action = FindActionSlots(actionName);
        if (!action)
        {
            return AZStd::nullopt;
        }

        const AZStd::string& channel = action->m_channels[static_cast<size_t>(group)][static_cast<size_t>(slot)];
        if (channel.empty())
        {
            return AZStd::nullopt;
        }
        return SlotBinding{ channel, InputChannelText::GetDisplayText(channel).value_or(channel) };
    }

    AZStd::optional<BindingConflict> RebindableInputSystemComponent::FindConflict(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName) const
    {
        if (channelName.empty())
        {
            return AZStd::nullopt;
        }

        const size_t g = static_cast<size_t>(group);
        for (const AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            for (const ActionSlots& action : entry->m_actions)
            {
                for (size_t s = 0; s < BindingSlotCount; ++s)
                {
                    const bool isTargetSlot = action.m_actionName == actionName && s == static_cast<size_t>(slot);
                    if (!isTargetSlot && action.m_channels[g][s] == channelName)
                    {
                        return BindingConflict{ action.m_actionName, static_cast<BindingSlot>(s) };
                    }
                }
            }
        }
        return AZStd::nullopt;
    }

    SetBindingResult RebindableInputSystemComponent::SetBinding(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName, ConflictMode mode)
    {
        const ActionSlots* action = FindActionSlots(actionName);
        if (!action)
        {
            return SetBindingResult::UnknownAction;
        }

        const AZStd::optional<ChannelInfo> info = InputChannelInfo::Find(channelName);
        if (!info)
        {
            return SetBindingResult::UnknownChannel;
        }
        if (info->m_deviceGroup != group)
        {
            return SetBindingResult::WrongDeviceGroup;
        }

        // The kind the action expects: from its defaults in this device group. Without defaults there, a button
        // action in the other group allows buttons here; an axis action cannot be moved to another device.
        const size_t g = static_cast<size_t>(group);
        AZStd::optional<ChannelKind> expectedKind;
        for (const AZStd::string& defaultChannel : action->m_defaultChannels[g])
        {
            if (const AZStd::optional<ChannelInfo> defaultInfo = InputChannelInfo::Find(defaultChannel); defaultInfo && !expectedKind)
            {
                expectedKind = defaultInfo->m_kind;
            }
        }
        if (!expectedKind)
        {
            expectedKind = ChannelKind::Button;
            for (const AZStd::string& otherDefault : action->m_defaultChannels[(g + 1) % DeviceGroupCount])
            {
                if (const AZStd::optional<ChannelInfo> otherInfo = InputChannelInfo::Find(otherDefault);
                    otherInfo && otherInfo->m_kind != ChannelKind::Button)
                {
                    return SetBindingResult::WrongChannelKind;
                }
            }
        }
        if (info->m_kind != *expectedKind)
        {
            return SetBindingResult::WrongChannelKind;
        }

        if (mode == ConflictMode::Move)
        {
            // Collected first: SetOverride must not run while the slots are being searched.
            AZStd::vector<BindingConflict> conflicts;
            for (const AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
            {
                for (const ActionSlots& other : entry->m_actions)
                {
                    for (size_t s = 0; s < BindingSlotCount; ++s)
                    {
                        const bool isTargetSlot = other.m_actionName == actionName && s == static_cast<size_t>(slot);
                        if (!isTargetSlot && other.m_channels[g][s] == channelName)
                        {
                            conflicts.push_back({ other.m_actionName, static_cast<BindingSlot>(s) });
                        }
                    }
                }
            }
            for (const BindingConflict& conflict : conflicts)
            {
                SetOverride(conflict.m_actionName, group, conflict.m_slot, AZStd::string());
            }
        }

        SetOverride(actionName, group, slot, channelName);
        RebuildAllBindings();
        NotifyInputHintsChanged();
        return SetBindingResult::Ok;
    }

    void RebindableInputSystemComponent::ClearBinding(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot)
    {
        if (!FindActionSlots(actionName))
        {
            return;
        }
        SetOverride(actionName, group, slot, AZStd::string());
        RebuildAllBindings();
        NotifyInputHintsChanged();
    }

    void RebindableInputSystemComponent::ResetAction(const AZStd::string& actionName)
    {
        AZStd::erase_if(m_overrides, [&](const PlayerOverride& change) { return change.m_actionName == actionName; });
        RebuildAllBindings();
        NotifyInputHintsChanged();
    }

    void RebindableInputSystemComponent::ResetAll()
    {
        m_overrides.clear();
        RebuildAllBindings();
        NotifyInputHintsChanged();
    }

    ////////////////////////////////////////////////////////////////////////
    // Lookups

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

    const RebindableInputSystemComponent::ActionSlots* RebindableInputSystemComponent::FindActionSlots(const AZStd::string& actionName) const
    {
        for (const AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            for (const ActionSlots& action : entry->m_actions)
            {
                if (action.m_actionName == actionName)
                {
                    return &action;
                }
            }
        }
        return nullptr;
    }

    AZStd::optional<InputHintsContract::InputHint> RebindableInputSystemComponent::GetInputHint(const AZStd::string& actionName) const
    {
        // The first filled keyboard and mouse slot of the action: the primary key, or the alternate one if the
        // player emptied the primary slot. Entities are searched in registration order.
        // TODO: pick by the device group the player uses right now (keyboard and mouse or gamepad).
        const size_t g = static_cast<size_t>(DeviceGroup::KeyboardMouse);
        for (const AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            for (const ActionSlots& action : entry->m_actions)
            {
                if (action.m_actionName != actionName)
                {
                    continue;
                }
                for (const AZStd::string& channel : action.m_channels[g])
                {
                    if (!channel.empty())
                    {
                        // A channel without text shows its raw name: a visible mistake (SetBindings already warned about it).
                        InputHintsContract::InputHint hint;
                        hint.m_text = InputChannelText::GetDisplayText(channel).value_or(channel);
                        return hint;
                    }
                }
            }
        }
        return AZStd::nullopt;
    }

    ////////////////////////////////////////////////////////////////////////
    // Warnings and helpers

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

    void RebindableInputSystemComponent::WarnAboutChannelsWithoutText([[maybe_unused]] const EntityBindings& entry)
    {
    #if defined(AZ_ENABLE_TRACING)
        // Checked once when the bindings are registered, not in GetInputHint: UI may ask for hints every frame.
        for (const InputEventGroup& group : entry.m_bindings.m_inputEventGroups)
        {
            for (const InputSubComponent* handler : group.m_inputHandlers)
            {
                const InputEventMap* inputEventMap = azrtti_cast<const InputEventMap*>(handler);
                AZ_Warning("RebindableInput", !inputEventMap || InputChannelText::GetDisplayText(inputEventMap->m_inputName),
                    "Entity %s, input action '%s': no display text for channel '%s', input hints show the channel name. "
                    "Add it to the table in InputChannelText.cpp.",
                    entry.m_entityId.ToString().c_str(), group.m_eventName.c_str(), inputEventMap->m_inputName.c_str());
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
        entry.m_actions.clear();
        // m_localUserId is kept: RebuildAllBindings activates the new clone for the same user.
    }

    void RebindableInputSystemComponent::FreeHandler(InputSubComponent* handler)
    {
        // The same as InputEventGroup::Cleanup: the serializer allocated the handler with azmalloc and placement new.
        handler->~InputSubComponent();
        azfree(static_cast<void*>(handler), AZ::SystemAllocator);
    }

} // namespace RebindableInput
