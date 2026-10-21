
#include <RebindableInput/RebindableInputTypeIds.h>
#include <RebindableInputModuleInterface.h>
#include "RebindableInputSystemComponent.h"

namespace RebindableInput
{
    class RebindableInputModule
        : public RebindableInputModuleInterface
    {
    public:
        AZ_RTTI(RebindableInputModule, RebindableInputModuleTypeId, RebindableInputModuleInterface);
        AZ_CLASS_ALLOCATOR(RebindableInputModule, AZ::SystemAllocator);
    };
}// namespace RebindableInput

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), RebindableInput::RebindableInputModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_RebindableInput, RebindableInput::RebindableInputModule)
#endif
