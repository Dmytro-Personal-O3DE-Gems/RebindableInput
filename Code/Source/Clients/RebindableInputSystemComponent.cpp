
#include "RebindableInputSystemComponent.h"

#include <RebindableInput/RebindableInputTypeIds.h>

#include <InputEventGroup.h>
#include <InputEventMap.h>
#include <RebindableInputComponent.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Serialization/SerializeContext.h>

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
    }

    void RebindableInputSystemComponent::Deactivate()
    {
        RebindableInputRequestBus::Handler::BusDisconnect();

        // Unregistered from the asset manager by AZ::Data::AssetHandler's destructor.
        m_inputEventBindingsAssetHandler.reset();
    }

} // namespace RebindableInput
