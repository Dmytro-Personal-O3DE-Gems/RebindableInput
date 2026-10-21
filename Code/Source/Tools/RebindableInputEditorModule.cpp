
#include <RebindableInput/RebindableInputTypeIds.h>
#include <RebindableInputModuleInterface.h>
#include "RebindableInputEditorSystemComponent.h"

namespace RebindableInput
{
    class RebindableInputEditorModule
        : public RebindableInputModuleInterface
    {
    public:
        AZ_RTTI(RebindableInputEditorModule, RebindableInputEditorModuleTypeId, RebindableInputModuleInterface);
        AZ_CLASS_ALLOCATOR(RebindableInputEditorModule, AZ::SystemAllocator);

        RebindableInputEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                RebindableInputEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<RebindableInputEditorSystemComponent>(),
            };
        }
    };
}// namespace RebindableInput

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), RebindableInput::RebindableInputEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_RebindableInput_Editor, RebindableInput::RebindableInputEditorModule)
#endif
