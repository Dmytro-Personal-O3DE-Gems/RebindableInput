
#pragma once

#include <AzCore/std/optional.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace RebindableInput
{
    //! The text of a key for the player: a localization key and the English text.
    //! Read like InteractionSystem::LocalizedText: an empty key means "show the text as it is";
    //! otherwise the translation of the key, or the English text if there is none.
    struct KeyText
    {
        AZStd::string m_key;         //!< e.g. "input.key.space"; empty for letters, digits, F-keys, symbols, gamepad A/B/X/Y
        AZStd::string m_defaultText; //!< English, e.g. "Space", "E"
    };

    //! Turns an input channel name (e.g. "keyboard_key_edit_space") into the text shown to the player.
    //!
    //! Stateless, so it is unit tested on its own (Tests/InputChannelTextTest.cpp).
    //! Rules, in this order:
    //!   1. a table of exact names: word keys ("Space"), numpad ("Num 5"), punctuation (","), mouse ("LMB"), gamepad ("A", "RT");
    //!   2. letters and digits: the part after "keyboard_key_alphanumeric_" ("E", "7"), never translated;
    //!   3. function keys: "keyboard_key_function_F01" -> "F1" (the leading zero is dropped), never translated.
    //!
    //! This gem does not translate: the keys are marked with LOCALIZATION_KEY (LocalizationKeyMarker.h), and whoever
    //! shows the text translates it. Letters are shown as on a QWERTY keyboard; the keyboard layout is not taken into account yet.
    class InputChannelText
    {
    public:
        //! The key and English text for the channel, or an empty optional for a channel these rules do not know.
        static AZStd::optional<KeyText> GetText(AZStd::string_view channelName);

        //! Only the English text: for log messages and checks.
        static AZStd::optional<AZStd::string> GetDisplayText(AZStd::string_view channelName);

        //! Keys whose text may come from the player's keyboard layout instead of US QWERTY: letters and punctuation.
        //! The engine names keys by their position (scan code) on a US QWERTY keyboard, so on AZERTY the key printed "A"
        //! is "keyboard_key_alphanumeric_Q". Digits are not included: AZERTY gives "&" for the "1" key without Shift,
        //! and the digit printed on the key is clearer. Word keys are not included: their text follows the game's language.
        static bool UsesLayoutText(AZStd::string_view channelName);

        //! The text of the key in the player's layout (from the engine), if it can be shown; otherwise the QWERTY text.
        //! Accepted: one printable ASCII character ("A", ";", "<"). Not accepted: other alphabets ("Ж" on a Russian
        //! layout: Latin is printed on those keys too), names of dead keys ("ACUTE"), an empty text (no layout text,
        //! e.g. on Linux). Letters from the engine are already upper case (GetKeyNameTextW).
        static AZStd::string ChooseLayoutText(AZStd::string_view qwertyText, AZStd::string_view layoutText);
    };
} // namespace RebindableInput
