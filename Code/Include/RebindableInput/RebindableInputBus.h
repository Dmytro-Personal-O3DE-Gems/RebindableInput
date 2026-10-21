
#pragma once

#include <RebindableInput/RebindableInputTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace RebindableInput
{
    class RebindableInputRequests
    {
    public:
        AZ_RTTI(RebindableInputRequests, RebindableInputRequestsTypeId);
        virtual ~RebindableInputRequests() = default;
        // Put your public methods here
    };

    class RebindableInputBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using RebindableInputRequestBus = AZ::EBus<RebindableInputRequests, RebindableInputBusTraits>;
    using RebindableInputInterface = AZ::Interface<RebindableInputRequests>;

} // namespace RebindableInput
