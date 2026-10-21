
#pragma once

namespace RebindableInput
{
    // System Component TypeIds
    inline constexpr const char* RebindableInputSystemComponentTypeId = "{572828FE-3582-46FB-835A-03D4D709AD3B}";
    inline constexpr const char* RebindableInputEditorSystemComponentTypeId = "{6F8C9D79-9C30-4B67-A3D9-56CE77F037F8}";

    // Module derived classes TypeIds
    inline constexpr const char* RebindableInputModuleInterfaceTypeId = "{71B3018F-7BA2-4B1B-83E5-F1E739347EAE}";
    inline constexpr const char* RebindableInputModuleTypeId = "{01E4901C-3372-49E4-AF5D-97AFD3F2C01C}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* RebindableInputEditorModuleTypeId = RebindableInputModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* RebindableInputRequestsTypeId = "{B660F152-9483-4E63-98DF-B2BEF7F7723C}";
} // namespace RebindableInput
