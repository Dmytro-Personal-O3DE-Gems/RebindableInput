
#include <InputChannelText.h>

#include <LocalizationKeyMarker.h>

namespace RebindableInput
{
    namespace
    {
        struct ChannelText
        {
            AZStd::string_view m_channelName;     //!< as in AzFramework, e.g. "keyboard_key_edit_space"
            AZStd::string_view m_localizationKey; //!< e.g. "input.key.space"; empty: the text is the same in every language
            AZStd::string_view m_defaultText;     //!< English, e.g. "Space"
        };

        // Channel names from AzFramework (InputDeviceKeyboard.h, InputDeviceMouse.h, InputDeviceGamepad.h, tag 2605.0).
        // Letters, digits, F1-F20 and numpad digits are not listed: they follow simple rules (see GetDisplayText).
        // Left and right modifiers share one text: the player needs the key, not the side.
        constexpr ChannelText ChannelTexts[] = {
            // Keyboard: edit
            { "keyboard_key_edit_backspace", LOCALIZATION_KEY("input.key.backspace"), "Backspace" },
            { "keyboard_key_edit_capslock", LOCALIZATION_KEY("input.key.caps_lock"), "Caps Lock" },
            { "keyboard_key_edit_enter", LOCALIZATION_KEY("input.key.enter"), "Enter" },
            { "keyboard_key_edit_space", LOCALIZATION_KEY("input.key.space"), "Space" },
            { "keyboard_key_edit_tab", LOCALIZATION_KEY("input.key.tab"), "Tab" },
            { "keyboard_key_escape", LOCALIZATION_KEY("input.key.escape"), "Esc" },

            // Keyboard: modifiers
            { "keyboard_key_modifier_alt_l", LOCALIZATION_KEY("input.key.alt"), "Alt" },
            { "keyboard_key_modifier_alt_r", LOCALIZATION_KEY("input.key.alt"), "Alt" },
            { "keyboard_key_modifier_ctrl_l", LOCALIZATION_KEY("input.key.ctrl"), "Ctrl" },
            { "keyboard_key_modifier_ctrl_r", LOCALIZATION_KEY("input.key.ctrl"), "Ctrl" },
            { "keyboard_key_modifier_shift_l", LOCALIZATION_KEY("input.key.shift"), "Shift" },
            { "keyboard_key_modifier_shift_r", LOCALIZATION_KEY("input.key.shift"), "Shift" },
            { "keyboard_key_modifier_super_l", LOCALIZATION_KEY("input.key.super"), "Win" },
            { "keyboard_key_modifier_super_r", LOCALIZATION_KEY("input.key.super"), "Win" },

            // Keyboard: navigation
            { "keyboard_key_navigation_arrow_down", LOCALIZATION_KEY("input.key.arrow_down"), "Down" },
            { "keyboard_key_navigation_arrow_left", LOCALIZATION_KEY("input.key.arrow_left"), "Left" },
            { "keyboard_key_navigation_arrow_right", LOCALIZATION_KEY("input.key.arrow_right"), "Right" },
            { "keyboard_key_navigation_arrow_up", LOCALIZATION_KEY("input.key.arrow_up"), "Up" },
            { "keyboard_key_navigation_delete", LOCALIZATION_KEY("input.key.delete"), "Delete" },
            { "keyboard_key_navigation_end", LOCALIZATION_KEY("input.key.end"), "End" },
            { "keyboard_key_navigation_home", LOCALIZATION_KEY("input.key.home"), "Home" },
            { "keyboard_key_navigation_insert", LOCALIZATION_KEY("input.key.insert"), "Insert" },
            { "keyboard_key_navigation_page_down", LOCALIZATION_KEY("input.key.page_down"), "Page Down" },
            { "keyboard_key_navigation_page_up", LOCALIZATION_KEY("input.key.page_up"), "Page Up" },

            // Keyboard: numpad. Digits are in the table too: "Num 5" is a phrase, and the order of its words may differ.
            { "keyboard_key_numpad_0", LOCALIZATION_KEY("input.key.numpad_0"), "Num 0" },
            { "keyboard_key_numpad_1", LOCALIZATION_KEY("input.key.numpad_1"), "Num 1" },
            { "keyboard_key_numpad_2", LOCALIZATION_KEY("input.key.numpad_2"), "Num 2" },
            { "keyboard_key_numpad_3", LOCALIZATION_KEY("input.key.numpad_3"), "Num 3" },
            { "keyboard_key_numpad_4", LOCALIZATION_KEY("input.key.numpad_4"), "Num 4" },
            { "keyboard_key_numpad_5", LOCALIZATION_KEY("input.key.numpad_5"), "Num 5" },
            { "keyboard_key_numpad_6", LOCALIZATION_KEY("input.key.numpad_6"), "Num 6" },
            { "keyboard_key_numpad_7", LOCALIZATION_KEY("input.key.numpad_7"), "Num 7" },
            { "keyboard_key_numpad_8", LOCALIZATION_KEY("input.key.numpad_8"), "Num 8" },
            { "keyboard_key_numpad_9", LOCALIZATION_KEY("input.key.numpad_9"), "Num 9" },
            { "keyboard_key_num_lock", LOCALIZATION_KEY("input.key.num_lock"), "Num Lock" },
            { "keyboard_key_numpad_add", LOCALIZATION_KEY("input.key.numpad_add"), "Num +" },
            { "keyboard_key_numpad_decimal", LOCALIZATION_KEY("input.key.numpad_decimal"), "Num ." },
            { "keyboard_key_numpad_divide", LOCALIZATION_KEY("input.key.numpad_divide"), "Num /" },
            { "keyboard_key_numpad_enter", LOCALIZATION_KEY("input.key.numpad_enter"), "Num Enter" },
            { "keyboard_key_numpad_multiply", LOCALIZATION_KEY("input.key.numpad_multiply"), "Num *" },
            { "keyboard_key_numpad_subtract", LOCALIZATION_KEY("input.key.numpad_subtract"), "Num -" },

            // Keyboard: punctuation (the symbols of a US QWERTY keyboard)
            { "keyboard_key_punctuation_apostrophe", "", "'" },
            { "keyboard_key_punctuation_backslash", "", "\\" },
            { "keyboard_key_punctuation_bracket_l", "", "[" },
            { "keyboard_key_punctuation_bracket_r", "", "]" },
            { "keyboard_key_punctuation_comma", "", "," },
            { "keyboard_key_punctuation_equals", "", "=" },
            { "keyboard_key_punctuation_hyphen", "", "-" },
            { "keyboard_key_punctuation_period", "", "." },
            { "keyboard_key_punctuation_semicolon", "", ";" },
            { "keyboard_key_punctuation_slash", "", "/" },
            { "keyboard_key_punctuation_tilde", "", "~" },
            { "keyboard_key_supplementary_iso", "", "ISO" },

            // Keyboard: system
            { "keyboard_key_windows_system_pause", LOCALIZATION_KEY("input.key.pause"), "Pause" },
            { "keyboard_key_windows_system_print", LOCALIZATION_KEY("input.key.print_screen"), "Print Screen" },
            { "keyboard_key_windows_system_scroll_lock", LOCALIZATION_KEY("input.key.scroll_lock"), "Scroll Lock" },

            // Mouse
            { "mouse_button_left", LOCALIZATION_KEY("input.mouse.left"), "LMB" },
            { "mouse_button_right", LOCALIZATION_KEY("input.mouse.right"), "RMB" },
            { "mouse_button_middle", LOCALIZATION_KEY("input.mouse.middle"), "MMB" },
            { "mouse_button_other1", LOCALIZATION_KEY("input.mouse.button4"), "Mouse 4" },
            { "mouse_button_other2", LOCALIZATION_KEY("input.mouse.button5"), "Mouse 5" },
            { "mouse_delta_x", LOCALIZATION_KEY("input.mouse.x"), "Mouse X" },
            { "mouse_delta_y", LOCALIZATION_KEY("input.mouse.y"), "Mouse Y" },
            { "mouse_delta_z", LOCALIZATION_KEY("input.mouse.wheel"), "Mouse Wheel" },

            // Gamepad (Xbox names)
            { "gamepad_button_a", "", "A" },
            { "gamepad_button_b", "", "B" },
            { "gamepad_button_x", "", "X" },
            { "gamepad_button_y", "", "Y" },
            { "gamepad_button_l1", "", "LB" },
            { "gamepad_button_r1", "", "RB" },
            { "gamepad_button_l3", "", "LS" },
            { "gamepad_button_r3", "", "RS" },
            { "gamepad_button_d_up", LOCALIZATION_KEY("input.gamepad.dpad_up"), "D-Pad Up" },
            { "gamepad_button_d_down", LOCALIZATION_KEY("input.gamepad.dpad_down"), "D-Pad Down" },
            { "gamepad_button_d_left", LOCALIZATION_KEY("input.gamepad.dpad_left"), "D-Pad Left" },
            { "gamepad_button_d_right", LOCALIZATION_KEY("input.gamepad.dpad_right"), "D-Pad Right" },
            { "gamepad_button_start", LOCALIZATION_KEY("input.gamepad.start"), "Start" },
            { "gamepad_button_select", LOCALIZATION_KEY("input.gamepad.select"), "Back" },
            { "gamepad_trigger_l2", "", "LT" },
            { "gamepad_trigger_r2", "", "RT" },
            { "gamepad_thumbstick_l", LOCALIZATION_KEY("input.gamepad.left_stick"), "Left Stick" },
            { "gamepad_thumbstick_r", LOCALIZATION_KEY("input.gamepad.right_stick"), "Right Stick" },
            { "gamepad_thumbstick_l_x", LOCALIZATION_KEY("input.gamepad.left_stick_x"), "Left Stick X" },
            { "gamepad_thumbstick_l_y", LOCALIZATION_KEY("input.gamepad.left_stick_y"), "Left Stick Y" },
            { "gamepad_thumbstick_r_x", LOCALIZATION_KEY("input.gamepad.right_stick_x"), "Right Stick X" },
            { "gamepad_thumbstick_r_y", LOCALIZATION_KEY("input.gamepad.right_stick_y"), "Right Stick Y" },
            { "gamepad_thumbstick_l_up", LOCALIZATION_KEY("input.gamepad.left_stick_up"), "Left Stick Up" },
            { "gamepad_thumbstick_l_down", LOCALIZATION_KEY("input.gamepad.left_stick_down"), "Left Stick Down" },
            { "gamepad_thumbstick_l_left", LOCALIZATION_KEY("input.gamepad.left_stick_left"), "Left Stick Left" },
            { "gamepad_thumbstick_l_right", LOCALIZATION_KEY("input.gamepad.left_stick_right"), "Left Stick Right" },
            { "gamepad_thumbstick_r_up", LOCALIZATION_KEY("input.gamepad.right_stick_up"), "Right Stick Up" },
            { "gamepad_thumbstick_r_down", LOCALIZATION_KEY("input.gamepad.right_stick_down"), "Right Stick Down" },
            { "gamepad_thumbstick_r_left", LOCALIZATION_KEY("input.gamepad.right_stick_left"), "Right Stick Left" },
            { "gamepad_thumbstick_r_right", LOCALIZATION_KEY("input.gamepad.right_stick_right"), "Right Stick Right" },
        };

        constexpr AZStd::string_view AlphanumericPrefix = "keyboard_key_alphanumeric_";
        constexpr AZStd::string_view FunctionPrefix = "keyboard_key_function_F";

        //! The part of name after prefix, or an empty view if name does not start with prefix.
        AZStd::string_view AfterPrefix(AZStd::string_view name, AZStd::string_view prefix)
        {
            if (name.size() > prefix.size() && name.substr(0, prefix.size()) == prefix)
            {
                return name.substr(prefix.size());
            }
            return {};
        }

        bool IsAllDigits(AZStd::string_view text)
        {
            if (text.empty())
            {
                return false;
            }
            for (char c : text)
            {
                if (c < '0' || c > '9')
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    AZStd::optional<KeyText> InputChannelText::GetText(AZStd::string_view channelName)
    {
        for (const ChannelText& entry : ChannelTexts)
        {
            if (entry.m_channelName == channelName)
            {
                return KeyText{ AZStd::string(entry.m_localizationKey), AZStd::string(entry.m_defaultText) };
            }
        }

        // "keyboard_key_alphanumeric_E" -> "E", "keyboard_key_alphanumeric_7" -> "7"
        if (AZStd::string_view key = AfterPrefix(channelName, AlphanumericPrefix); !key.empty())
        {
            return KeyText{ AZStd::string(), AZStd::string(key) };
        }

        // "keyboard_key_function_F01" -> "F1": the engine names have two digits, players expect no leading zero.
        if (AZStd::string_view number = AfterPrefix(channelName, FunctionPrefix); IsAllDigits(number))
        {
            while (number.size() > 1 && number.front() == '0')
            {
                number.remove_prefix(1);
            }
            return KeyText{ AZStd::string(), AZStd::string("F") + AZStd::string(number) };
        }

        return AZStd::nullopt;
    }

    bool InputChannelText::UsesLayoutText(AZStd::string_view channelName)
    {
        constexpr AZStd::string_view PunctuationPrefix = "keyboard_key_punctuation_";
        constexpr AZStd::string_view IsoKey = "keyboard_key_supplementary_iso"; // the extra key of ISO keyboards, e.g. "<"

        const AZStd::string_view key = AfterPrefix(channelName, AlphanumericPrefix);
        const bool isLetter = key.size() == 1 && key[0] >= 'A' && key[0] <= 'Z';
        return isLetter || !AfterPrefix(channelName, PunctuationPrefix).empty() || channelName == IsoKey;
    }

    AZStd::string InputChannelText::ChooseLayoutText(AZStd::string_view qwertyText, AZStd::string_view layoutText)
    {
        // Printable ASCII without the space: '!' (0x21) to '~' (0x7E).
        if (layoutText.size() == 1 && layoutText[0] >= '!' && layoutText[0] <= '~')
        {
            return AZStd::string(layoutText);
        }
        return AZStd::string(qwertyText);
    }

    AZStd::optional<AZStd::string> InputChannelText::GetDisplayText(AZStd::string_view channelName)
    {
        if (AZStd::optional<KeyText> text = GetText(channelName))
        {
            return AZStd::move(text->m_defaultText);
        }
        return AZStd::nullopt;
    }
} // namespace RebindableInput
