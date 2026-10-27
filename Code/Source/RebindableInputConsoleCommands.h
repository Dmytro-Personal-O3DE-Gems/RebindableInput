
#pragma once

#include <AzCore/Console/IConsole.h>

namespace RebindableInput
{
    //! Debug console commands for the rebinding API:
    //!   rebind_list [action]
    //!   rebind_set <action> <KeyboardMouse|km|Gamepad|gp> <Primary|p|Alternate|a> <channel> [Move|Duplicate]
    //!   rebind_clear <action> <group> <slot>
    //!   rebind_reset [action]
    //! They call RebindableInputInterface the way a settings menu would, so they test the public API.
    //!
    //! The commands are members, not AZ_CONSOLEFREEFUNC globals: the gem code is a static library, and the linker
    //! drops an object file that nothing references, global console commands included.
    //! They exist (are registered in the console) while this object exists: RebindableInputSystemComponent creates it
    //! in Activate and destroys it in Deactivate.
    class RebindableInputConsoleCommands
    {
    public:
        RebindableInputConsoleCommands();

    private:
        AZ::ConsoleFunctor<void, false> m_list;
        AZ::ConsoleFunctor<void, false> m_set;
        AZ::ConsoleFunctor<void, false> m_clear;
        AZ::ConsoleFunctor<void, false> m_reset;
    };

} // namespace RebindableInput
