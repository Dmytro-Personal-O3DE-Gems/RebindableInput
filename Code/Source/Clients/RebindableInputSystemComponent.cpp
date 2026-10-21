
#include "RebindableInputSystemComponent.h"

#include <RebindableInput/RebindableInputTypeIds.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace RebindableInput
{
    AZ_COMPONENT_IMPL(RebindableInputSystemComponent, "RebindableInputSystemComponent",
        RebindableInputSystemComponentTypeId);

    void RebindableInputSystemComponent::Reflect(AZ::ReflectContext* context)
    {
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

    void RebindableInputSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
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
        RebindableInputRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
    }

    void RebindableInputSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        RebindableInputRequestBus::Handler::BusDisconnect();
    }

    void RebindableInputSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
    }

} // namespace RebindableInput
