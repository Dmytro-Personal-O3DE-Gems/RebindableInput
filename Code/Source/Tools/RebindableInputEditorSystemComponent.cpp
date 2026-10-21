
#include <AzCore/Serialization/SerializeContext.h>
#include "RebindableInputEditorSystemComponent.h"

#include <RebindableInput/RebindableInputTypeIds.h>

namespace RebindableInput
{
    AZ_COMPONENT_IMPL(RebindableInputEditorSystemComponent, "RebindableInputEditorSystemComponent",
        RebindableInputEditorSystemComponentTypeId, BaseSystemComponent);

    void RebindableInputEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<RebindableInputEditorSystemComponent, RebindableInputSystemComponent>()
                ->Version(0);
        }
    }

    RebindableInputEditorSystemComponent::RebindableInputEditorSystemComponent() = default;

    RebindableInputEditorSystemComponent::~RebindableInputEditorSystemComponent() = default;

    void RebindableInputEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("RebindableInputEditorService"));
    }

    void RebindableInputEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("RebindableInputEditorService"));
    }

    void RebindableInputEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void RebindableInputEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void RebindableInputEditorSystemComponent::Activate()
    {
        RebindableInputSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void RebindableInputEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        RebindableInputSystemComponent::Deactivate();
    }

} // namespace RebindableInput
