
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/RebindableInputSystemComponent.h>

namespace RebindableInput
{
    /// System component for RebindableInput editor
    class RebindableInputEditorSystemComponent
        : public RebindableInputSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = RebindableInputSystemComponent;
    public:
        AZ_COMPONENT_DECL(RebindableInputEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        RebindableInputEditorSystemComponent();
        ~RebindableInputEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace RebindableInput
