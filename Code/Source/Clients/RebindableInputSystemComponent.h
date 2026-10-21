
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Asset/GenericAssetHandler.h>
#include <RebindableInput/RebindableInputBus.h>

#include <InputEventBindings.h>

namespace RebindableInput
{
    //! Reflects the binding classes and registers the .keybinds asset type,
    //! like StartingPointInputSystemComponent does for .inputbindings.
    class RebindableInputSystemComponent
        : public AZ::Component
        , protected RebindableInputRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(RebindableInputSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        RebindableInputSystemComponent();
        ~RebindableInputSystemComponent();

    protected:
        ////////////////////////////////////////////////////////////////////////
        // RebindableInputRequestBus interface implementation

        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        //! Loads and saves .keybinds files. Requires the asset manager ("AssetDatabaseService").
        AZStd::unique_ptr<AzFramework::GenericAssetHandler<InputEventBindingsAsset>> m_inputEventBindingsAssetHandler;
    };

} // namespace RebindableInput
