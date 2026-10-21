
#include "RebindableInputModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <RebindableInput/RebindableInputTypeIds.h>

#include <Clients/RebindableInputSystemComponent.h>

namespace RebindableInput
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(RebindableInputModuleInterface,
        "RebindableInputModuleInterface", RebindableInputModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(RebindableInputModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(RebindableInputModuleInterface, AZ::SystemAllocator);

    RebindableInputModuleInterface::RebindableInputModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            RebindableInputSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList RebindableInputModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<RebindableInputSystemComponent>(),
        };
    }
} // namespace RebindableInput
