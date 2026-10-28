
#include "RebindableInputSystemComponent.h"

#include <RebindableInput/RebindableInputTypeIds.h>

#include <ActiveDeviceTracker.h>
#include <InputCapture.h>
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
#include <AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzCore/std/smart_ptr/weak_ptr.h>
#include <InputHintsContract/InputHintsContractNotificationBus.h>
#include <RebindableInput/RebindableInputNotificationBus.h>

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
        // The player's file is JSON written from these structs.
        PlayerBindingsFile::Reflect(context);

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

    void RebindableInputSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        // The player's file is loaded in Activate. SaveData connects to its bus in its own Activate, so it must come first
        // where it exists. Dependent, not required: SaveData runs only in game launchers, the Editor works without it.
        dependent.push_back(AZ_CRC_CE("SaveDataService"));
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

        // The player's file: after everything above, because the answer rebuilds the bindings and notifies UI.
        // With SaveData the answer comes later; the guard drops it if this component is deactivated before.
        m_callbackGuard = AZStd::make_shared<int>(0);
        LoadPlayerBindings();

        m_inputCapture = AZStd::make_unique<InputCapture>();

        // Which device group the player uses: the input hints show the keys of that group.
        m_activeDeviceTracker = AZStd::make_unique<ActiveDeviceTracker>([](DeviceGroup group)
        {
            RebindableInputNotificationBus::Broadcast(&RebindableInputNotifications::OnActiveDeviceGroupChanged, group);
            NotifyInputHintsChanged();
        });

#if defined(AZ_ENABLE_TRACING)
        // After the rebinding API is ready: the commands only call it.
        // Only in builds with tracing (debug, profile): debug commands that change the bindings are not for players.
        m_consoleCommands = AZStd::make_unique<RebindableInputConsoleCommands>();
#endif
    }

    void RebindableInputSystemComponent::Deactivate()
    {
        // Unregistered from the console by ConsoleFunctorBase's destructor.
        m_consoleCommands.reset();

        // Stops listening to input; a capture in progress ends without a notification.
        m_inputCapture.reset();
        m_activeDeviceTracker.reset();

        // Answers of the storage that arrive from now on are ignored. The player's state is loaded again on the next Activate.
        m_callbackGuard.reset();
        m_loadingPlayerBindings = false;
        m_overrides.clear();
        m_savedOverrides.clear();
        m_knownDefaults.clear();

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

    void RebindableInputSystemComponent::BuildBindings(EntityBindings& entry, bool registering)
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
        AZStd::vector<SlotRecord> resolvedConflicts;
        for (InputEventGroup& group : entry.m_bindings.m_inputEventGroups)
        {
            ActionSlots& action = entry.m_actions.emplace_back();
            action.m_actionName = group.m_eventName;
            ApplySlots(group, action, *serializeContext, entry.m_entityId, registering, resolvedConflicts);
        }

        if (registering)
        {
            // A resolved conflict becomes a change of the player, also in the saved state: RevertBindings must not
            // bring the conflict back, and after the defaults are remembered below, the next save keeps the slot empty.
            for (const SlotRecord& conflict : resolvedConflicts)
            {
                m_overrides.push_back(conflict);
                m_savedOverrides.push_back(conflict);
            }
            for (const ActionSlots& action : entry.m_actions)
            {
                RememberDefaults(action);
            }
        }

        if (entry.m_localUserId != AzFramework::LocalUserIdNone)
        {
            entry.m_bindings.Activate(entry.m_localUserId);
        }
    }

    void RebindableInputSystemComponent::ApplySlots(
        InputEventGroup& group, ActionSlots& action, AZ::SerializeContext& serializeContext,
        [[maybe_unused]] AZ::EntityId entityId, bool registering, AZStd::vector<SlotRecord>& resolvedConflicts) const
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
                AZ_Warning("RebindableInput", !registering,
                    "Entity %s, input action '%s': more than %zu bindings for one device group, '%s' is disabled. "
                    "Only the first two (primary and alternate) are used.",
                    entityId.ToString().c_str(), group.m_eventName.c_str(), BindingSlotCount, inputEventMap->m_inputName.c_str());
                FreeHandler(handler);
            }
        }

        // 2. The player's changes on top of the defaults.
        action.m_channels = action.m_defaultChannels;
        AZStd::array<AZStd::array<bool, BindingSlotCount>, DeviceGroupCount> changedByPlayer{};
        for (const SlotRecord& change : m_overrides)
        {
            if (change.m_actionName != action.m_actionName)
            {
                continue;
            }
            // SetBinding checks a change before storing it. A change from the player's file is checked here, where the
            // action is known: the action may expect another kind of channel since the player saved (a game update).
            if (!change.m_channelName.empty() && !AcceptsChannel(action, change.m_group, change.m_channelName))
            {
                AZ_Warning("RebindableInput", !registering,
                    "Input action '%s': the player's '%s' (%s %s) does not fit the action any more, the default is used.",
                    action.m_actionName.c_str(), change.m_channelName.c_str(),
                    PlayerBindingsFile::ToString(change.m_group), PlayerBindingsFile::ToString(change.m_slot));
                continue;
            }
            const size_t g = static_cast<size_t>(change.m_group);
            const size_t s = static_cast<size_t>(change.m_slot);
            action.m_channels[g][s] = change.m_channelName;
            changedByPlayer[g][s] = true;
        }

        // 3. A default that is new to the player (a game update, a new action, a reloaded asset) and uses a key the player
        //    took for another slot: the player's choice wins, the default slot is emptied.
        //    Only when registering: after the player's own changes in this session the defaults are already known, and a
        //    duplicate is then intended (ConflictMode::AllowDuplicate).
        if (registering)
        {
            for (size_t g = 0; g < DeviceGroupCount; ++g)
            {
                for (size_t s = 0; s < BindingSlotCount; ++s)
                {
                    const AZStd::string& defaultChannel = action.m_defaultChannels[g][s];
                    if (changedByPlayer[g][s] || defaultChannel.empty())
                    {
                        continue;
                    }
                    const DeviceGroup deviceGroup = static_cast<DeviceGroup>(g);
                    const BindingSlot slot = static_cast<BindingSlot>(s);
                    const AZStd::string* knownDefault = FindKnownDefault(action.m_actionName, deviceGroup, slot);
                    if (knownDefault && *knownDefault == defaultChannel)
                    {
                        continue; // the player has seen this default: a duplicate with it is the player's choice
                    }
                    if (IsTakenByPlayer(deviceGroup, defaultChannel))
                    {
                        AZ_Warning("RebindableInput", false,
                            "Input action '%s': the default '%s' (%s %s) is new, but the player uses this key for another slot. "
                            "The player's choice wins: the slot is left empty.",
                            action.m_actionName.c_str(), defaultChannel.c_str(),
                            PlayerBindingsFile::ToString(deviceGroup), PlayerBindingsFile::ToString(slot));
                        action.m_channels[g][s].clear();
                        resolvedConflicts.push_back({ action.m_actionName, deviceGroup, slot, AZStd::string() });
                    }
                }
            }
        }

        // 4. Make the handlers match the slots.
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

        // 5. Put the handlers back: primary and alternate of each device group, then the rest.
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

    void RebindableInputSystemComponent::RebuildAllBindings(bool registering)
    {
        for (AZStd::unique_ptr<EntityBindings>& entry : m_entityBindings)
        {
            ReleaseBindings(*entry);
            BuildBindings(*entry, registering);
        }
    }

    KeyText RebindableInputSystemComponent::GetKeyText(const AZStd::string& channelName)
    {
        // A channel without text shows its raw name: a visible mistake (SetBindings already warned about it).
        KeyText text = InputChannelText::GetText(channelName).value_or(KeyText{ AZStd::string(), channelName });

        if (InputChannelText::UsesLayoutText(channelName))
        {
            if (const AzFramework::InputChannelId* channelId = InputChannelInfo::FindKeyboardChannelId(channelName))
            {
                // Windows: GetKeyNameTextW by scan code (letters always Latin, A-Z). Linux: not implemented, stays empty.
                AZStd::string layoutText;
                AzFramework::InputDeviceRequestBus::Event(AzFramework::InputDeviceKeyboard::Id,
                    &AzFramework::InputDeviceRequests::GetPhysicalKeyOrButtonText, *channelId, layoutText);
                text.m_defaultText = InputChannelText::ChooseLayoutText(text.m_defaultText, layoutText);
            }
        }
        return text;
    }

    AZStd::optional<ChannelKind> RebindableInputSystemComponent::GetExpectedKind(const ActionSlots& action, DeviceGroup group)
    {
        const size_t g = static_cast<size_t>(group);
        for (const AZStd::string& defaultChannel : action.m_defaultChannels[g])
        {
            if (const AZStd::optional<ChannelInfo> defaultInfo = InputChannelInfo::Find(defaultChannel))
            {
                return defaultInfo->m_kind;
            }
        }
        for (const AZStd::string& otherDefault : action.m_defaultChannels[(g + 1) % DeviceGroupCount])
        {
            if (const AZStd::optional<ChannelInfo> otherInfo = InputChannelInfo::Find(otherDefault);
                otherInfo && otherInfo->m_kind != ChannelKind::Button)
            {
                return AZStd::nullopt;
            }
        }
        return ChannelKind::Button;
    }

    bool RebindableInputSystemComponent::AcceptsChannel(const ActionSlots& action, DeviceGroup group, const AZStd::string& channelName)
    {
        const AZStd::optional<ChannelInfo> info = InputChannelInfo::Find(channelName);
        if (!info || info->m_deviceGroup != group || InputCapture::IsReservedChannel(channelName))
        {
            return false;
        }
        const AZStd::optional<ChannelKind> expectedKind = GetExpectedKind(action, group);
        return expectedKind && info->m_kind == *expectedKind;
    }

    bool RebindableInputSystemComponent::IsTakenByPlayer(DeviceGroup group, const AZStd::string& channelName) const
    {
        return AZStd::any_of(m_overrides.begin(), m_overrides.end(), [&](const SlotRecord& change)
        {
            return change.m_group == group && change.m_channelName == channelName;
        });
    }

    const AZStd::string* RebindableInputSystemComponent::FindKnownDefault(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot) const
    {
        for (const SlotRecord& known : m_knownDefaults)
        {
            if (known.m_actionName == actionName && known.m_group == group && known.m_slot == slot)
            {
                return &known.m_channelName;
            }
        }
        return nullptr;
    }

    void RebindableInputSystemComponent::RememberDefaults(const ActionSlots& action)
    {
        AZStd::erase_if(m_knownDefaults, [&](const SlotRecord& known) { return known.m_actionName == action.m_actionName; });
        for (size_t g = 0; g < DeviceGroupCount; ++g)
        {
            for (size_t s = 0; s < BindingSlotCount; ++s)
            {
                // Empty defaults too: a key that later appears in an empty slot is new to the player.
                m_knownDefaults.push_back({ action.m_actionName, static_cast<DeviceGroup>(g), static_cast<BindingSlot>(s),
                    action.m_defaultChannels[g][s] });
            }
        }
    }

    void RebindableInputSystemComponent::SetOverride(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName)
    {
        AZStd::erase_if(m_overrides, [&](const SlotRecord& change)
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
        const KeyText text = GetKeyText(channel);
        return SlotBinding{ channel, text.m_key, text.m_defaultText };
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
        if (InputCapture::IsReservedChannel(channelName))
        {
            return SetBindingResult::ReservedChannel;
        }
        if (info->m_deviceGroup != group)
        {
            return SetBindingResult::WrongDeviceGroup;
        }

        const AZStd::optional<ChannelKind> expectedKind = GetExpectedKind(*action, group);
        if (!expectedKind || info->m_kind != *expectedKind)
        {
            return SetBindingResult::WrongChannelKind;
        }

        const size_t g = static_cast<size_t>(group);

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
        AZStd::erase_if(m_overrides, [&](const SlotRecord& change) { return change.m_actionName == actionName; });
        RebuildAllBindings();
        NotifyInputHintsChanged();
    }

    void RebindableInputSystemComponent::ResetAll()
    {
        m_overrides.clear();
        RebuildAllBindings();
        NotifyInputHintsChanged();
    }

    void RebindableInputSystemComponent::SaveBindings()
    {
        if (m_loadingPlayerBindings)
        {
            // Saving now would overwrite the file with the defaults before the player's changes arrive.
            AZ_Warning("RebindableInput", false, "SaveBindings: the player's bindings are still being loaded, nothing is saved.");
            NotifyBindingsSaved(false);
            return;
        }

        PlayerBindingsData data;
        data.m_overrides = m_overrides;
        data.m_defaults = m_knownDefaults;
        AZ::Outcome<AZStd::string, AZStd::string> json = PlayerBindingsFile::ToJson(data);
        if (!json.IsSuccess())
        {
            AZ_Error("RebindableInput", false, "SaveBindings: cannot write the player's bindings as JSON: %s", json.GetError().c_str());
            NotifyBindingsSaved(false);
            return;
        }

        // The changes as they are now: if the player changes more before SaveData answers, Revert returns to these.
        AZStd::weak_ptr<int> guard = m_callbackGuard;
        PlayerBindingsStorage::Save(json.TakeValue(), [this, guard, saved = AZStd::move(data.m_overrides)](bool success)
        {
            if (success && !guard.expired())
            {
                m_savedOverrides = saved;
            }
            if (success)
            {
                AZ_TracePrintf("RebindableInput", "Saved the player's bindings (%zu changes) to %s.\n",
                    saved.size(), PlayerBindingsStorage::GetLocation().c_str());
            }
            else
            {
                AZ_Warning("RebindableInput", false, "Cannot save the player's bindings to %s.", PlayerBindingsStorage::GetLocation().c_str());
            }
            NotifyBindingsSaved(success);
        });
    }

    void RebindableInputSystemComponent::RevertBindings()
    {
        m_overrides = m_savedOverrides;
        RebuildAllBindings();
        NotifyInputHintsChanged();
    }

    bool RebindableInputSystemComponent::StartCapture(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot)
    {
        const ActionSlots* action = FindActionSlots(actionName);
        if (!action || !m_inputCapture)
        {
            return false;
        }
        // Only the kind the slot accepts is taken: mouse movement must not end a capture for a button action.
        const AZStd::optional<ChannelKind> kind = GetExpectedKind(*action, group);
        if (!kind)
        {
            return false;
        }
        m_inputCapture->Start(actionName, group, slot, *kind);
        return true;
    }

    void RebindableInputSystemComponent::CancelCapture()
    {
        if (m_inputCapture)
        {
            m_inputCapture->Cancel();
        }
    }

    bool RebindableInputSystemComponent::IsCapturing() const
    {
        return m_inputCapture && m_inputCapture->IsCapturing();
    }

    DeviceGroup RebindableInputSystemComponent::GetActiveDeviceGroup() const
    {
        return m_activeDeviceTracker ? m_activeDeviceTracker->GetActiveGroup() : DeviceGroup::KeyboardMouse;
    }

    ////////////////////////////////////////////////////////////////////////
    // The player's file

    void RebindableInputSystemComponent::LoadPlayerBindings()
    {
        m_loadingPlayerBindings = true;
        AZStd::weak_ptr<int> guard = m_callbackGuard;
        PlayerBindingsStorage::Load([this, guard](PlayerBindingsStorage::LoadResult result, AZStd::string text)
        {
            if (!guard.expired())
            {
                OnPlayerBindingsLoaded(result, text);
            }
        });
    }

    void RebindableInputSystemComponent::OnPlayerBindingsLoaded(PlayerBindingsStorage::LoadResult result, const AZStd::string& text)
    {
        m_loadingPlayerBindings = false;

        if (result == PlayerBindingsStorage::LoadResult::NotFound)
        {
            AZ_TracePrintf("RebindableInput", "No saved player bindings yet (%s): the defaults are used.\n",
                PlayerBindingsStorage::GetLocation().c_str());
            return;
        }
        if (result == PlayerBindingsStorage::LoadResult::Failed)
        {
            AZ_Warning("RebindableInput", false, "Cannot read the player's bindings (%s): the defaults are used.",
                PlayerBindingsStorage::GetLocation().c_str());
            return;
        }

        AZStd::vector<AZStd::string> warnings;
        AZ::Outcome<PlayerBindingsData, AZStd::string> data = PlayerBindingsFile::FromJson(text, warnings);
    #if defined(AZ_ENABLE_TRACING)
        for (const AZStd::string& warning : warnings)
        {
            AZ_Warning("RebindableInput", false, "Player's bindings (%s): %s", PlayerBindingsStorage::GetLocation().c_str(), warning.c_str());
        }
    #endif // AZ_ENABLE_TRACING
        if (!data.IsSuccess())
        {
            AZ_Warning("RebindableInput", false,
                "The player's bindings (%s) are not a valid bindings file: %s. The defaults are used; the next save replaces the file.",
                PlayerBindingsStorage::GetLocation().c_str(), data.GetError().c_str());
            return;
        }

        PlayerBindingsData loaded = data.TakeValue();
        m_overrides = AZStd::move(loaded.m_overrides);
        m_savedOverrides = m_overrides;
        m_knownDefaults = AZStd::move(loaded.m_defaults);
        AZ_TracePrintf("RebindableInput", "Loaded the player's bindings (%zu changes) from %s.\n",
            m_overrides.size(), PlayerBindingsStorage::GetLocation().c_str());

        // Entities that registered before the file arrived (SaveData answers later) get the player's changes now.
        // As registration: their defaults are compared with the ones from the file.
        if (!m_entityBindings.empty())
        {
            RebuildAllBindings(true);
            NotifyInputHintsChanged();
        }
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
        // The first filled slot of the action in the device group the player uses right now: the primary key, or the
        // alternate one if the primary slot is empty. No key in this group: no hint (UI shows its "unbound" text),
        // even if the other group has one: the player has the device of this group in hand.
        // Entities are searched in registration order.
        const size_t g = static_cast<size_t>(GetActiveDeviceGroup());
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
                        // Not translated here: the UI translates the key (see InputHintsContract::InputHint).
                        const KeyText text = GetKeyText(channel);
                        InputHintsContract::InputHint hint;
                        hint.m_key = text.m_key;
                        hint.m_defaultText = text.m_defaultText;
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

    void RebindableInputSystemComponent::NotifyBindingsSaved(bool success)
    {
        RebindableInputNotificationBus::Broadcast(&RebindableInputNotifications::OnBindingsSaved, success);
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
