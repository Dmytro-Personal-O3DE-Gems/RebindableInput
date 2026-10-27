
#pragma once

#include <AzCore/std/optional.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace RebindableInput
{
    //! Turns an input channel name (e.g. "keyboard_key_alphanumeric_E") into the text shown to the player (e.g. "E").
    //!
    //! Stateless, so it is unit tested on its own (Tests/InputChannelTextTest.cpp).
    //! Rules, in this order:
    //!   1. a table of exact names: word keys ("Space"), punctuation (","), mouse ("LMB"), gamepad ("A", "RT");
    //!   2. letters and digits: the part after "keyboard_key_alphanumeric_" ("E", "7");
    //!   3. function keys: "keyboard_key_function_F01" -> "F1" (the leading zero is dropped);
    //!   4. numpad digits: "keyboard_key_numpad_5" -> "Num 5".
    //!
    //! English only for now. Every table entry already has a localization key (input.key.<name>),
    //! so translating the word keys later is a change in one place.
    //! Letters are shown as on a QWERTY keyboard; the player's keyboard layout is not taken into account yet.
    class InputChannelText
    {
    public:
        //! Returns the text for the channel, or an empty optional for a channel these rules do not know.
        static AZStd::optional<AZStd::string> GetDisplayText(AZStd::string_view channelName);
    };
} // namespace RebindableInput
