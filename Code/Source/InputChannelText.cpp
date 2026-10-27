
#include <InputChannelText.h>

namespace RebindableInput
{
    namespace
    {
        struct ChannelText
        {
            AZStd::string_view m_channelName;     //!< as in AzFramework, e.g. "keyboard_key_edit_space"
            AZStd::string_view m_localizationKey; //!< for translating later, e.g. "input.key.space"; not used yet
            AZStd::string_view m_defaultText;     //!< English text shown now, e.g. "Space"
        };

        // Channel names from AzFramework (InputDeviceKeyboard.h, InputDeviceMouse.h, InputDeviceGamepad.h, tag 2605.0).
        // Letters, digits, F1-F20 and numpad digits are not listed: they follow simple rules (see GetDisplayText).
        // Left and right modifiers share one text: the player needs the key, not the side.
        constexpr ChannelText ChannelTexts[] = {
            // Keyboard: edit
            { "keyboard_key_edit_backspace", "input.key.backspace", "Backspace" },
            { "keyboard_key_edit_capslock", "input.key.caps_lock", "Caps Lock" },
            { "keyboard_key_edit_enter", "input.key.enter", "Enter" },
            { "keyboard_key_edit_space", "input.key.space", "Space" },
            { "keyboard_key_edit_tab", "input.key.tab", "Tab" },
            { "keyboard_key_escape", "input.key.escape", "Esc" },

            // Keyboard: modifiers
            { "keyboard_key_modifier_alt_l", "input.key.alt", "Alt" },
            { "keyboard_key_modifier_alt_r", "input.key.alt", "Alt" },
            { "keyboard_key_modifier_ctrl_l", "input.key.ctrl", "Ctrl" },
            { "keyboard_key_modifier_ctrl_r", "input.key.ctrl", "Ctrl" },
            { "keyboard_key_modifier_shift_l", "input.key.shift", "Shift" },
            { "keyboard_key_modifier_shift_r", "input.key.shift", "Shift" },
            { "keyboard_key_modifier_super_l", "input.key.super", "Win" },
            { "keyboard_key_modifier_super_r", "input.key.super", "Win" },

            // Keyboard: navigation
            { "keyboard_key_navigation_arrow_down", "input.key.arrow_down", "Down" },
            { "keyboard_key_navigation_arrow_left", "input.key.arrow_left", "Left" },
            { "keyboard_key_navigation_arrow_right", "input.key.arrow_right", "Right" },
            { "keyboard_key_navigation_arrow_up", "input.key.arrow_up", "Up" },
            { "keyboard_key_navigation_delete", "input.key.delete", "Delete" },
            { "keyboard_key_navigation_end", "input.key.end", "End" },
            { "keyboard_key_navigation_home", "input.key.home", "Home" },
            { "keyboard_key_navigation_insert", "input.key.insert", "Insert" },
            { "keyboard_key_navigation_page_down", "input.key.page_down", "Page Down" },
            { "keyboard_key_navigation_page_up", "input.key.page_up", "Page Up" },

            // Keyboard: numpad (digits follow a rule)
            { "keyboard_key_num_lock", "input.key.num_lock", "Num Lock" },
            { "keyboard_key_numpad_add", "input.key.numpad_add", "Num +" },
            { "keyboard_key_numpad_decimal", "input.key.numpad_decimal", "Num ." },
            { "keyboard_key_numpad_divide", "input.key.numpad_divide", "Num /" },
            { "keyboard_key_numpad_enter", "input.key.numpad_enter", "Num Enter" },
            { "keyboard_key_numpad_multiply", "input.key.numpad_multiply", "Num *" },
            { "keyboard_key_numpad_subtract", "input.key.numpad_subtract", "Num -" },

            // Keyboard: punctuation (the symbols of a US QWERTY keyboard)
            { "keyboard_key_punctuation_apostrophe", "input.key.apostrophe", "'" },
            { "keyboard_key_punctuation_backslash", "input.key.backslash", "\\" },
            { "keyboard_key_punctuation_bracket_l", "input.key.bracket_left", "[" },
            { "keyboard_key_punctuation_bracket_r", "input.key.bracket_right", "]" },
            { "keyboard_key_punctuation_comma", "input.key.comma", "," },
            { "keyboard_key_punctuation_equals", "input.key.equals", "=" },
            { "keyboard_key_punctuation_hyphen", "input.key.hyphen", "-" },
            { "keyboard_key_punctuation_period", "input.key.period", "." },
            { "keyboard_key_punctuation_semicolon", "input.key.semicolon", ";" },
            { "keyboard_key_punctuation_slash", "input.key.slash", "/" },
            { "keyboard_key_punctuation_tilde", "input.key.tilde", "~" },
            { "keyboard_key_supplementary_iso", "input.key.iso", "ISO" },

            // Keyboard: system
            { "keyboard_key_windows_system_pause", "input.key.pause", "Pause" },
            { "keyboard_key_windows_system_print", "input.key.print_screen", "Print Screen" },
            { "keyboard_key_windows_system_scroll_lock", "input.key.scroll_lock", "Scroll Lock" },

            // Mouse
            { "mouse_button_left", "input.mouse.left", "LMB" },
            { "mouse_button_right", "input.mouse.right", "RMB" },
            { "mouse_button_middle", "input.mouse.middle", "MMB" },
            { "mouse_button_other1", "input.mouse.button4", "Mouse 4" },
            { "mouse_button_other2", "input.mouse.button5", "Mouse 5" },
            { "mouse_delta_x", "input.mouse.x", "Mouse X" },
            { "mouse_delta_y", "input.mouse.y", "Mouse Y" },
            { "mouse_delta_z", "input.mouse.wheel", "Mouse Wheel" },

            // Gamepad (Xbox names)
            { "gamepad_button_a", "input.gamepad.a", "A" },
            { "gamepad_button_b", "input.gamepad.b", "B" },
            { "gamepad_button_x", "input.gamepad.x", "X" },
            { "gamepad_button_y", "input.gamepad.y", "Y" },
            { "gamepad_button_l1", "input.gamepad.lb", "LB" },
            { "gamepad_button_r1", "input.gamepad.rb", "RB" },
            { "gamepad_button_l3", "input.gamepad.ls", "LS" },
            { "gamepad_button_r3", "input.gamepad.rs", "RS" },
            { "gamepad_button_d_up", "input.gamepad.dpad_up", "D-Pad Up" },
            { "gamepad_button_d_down", "input.gamepad.dpad_down", "D-Pad Down" },
            { "gamepad_button_d_left", "input.gamepad.dpad_left", "D-Pad Left" },
            { "gamepad_button_d_right", "input.gamepad.dpad_right", "D-Pad Right" },
            { "gamepad_button_start", "input.gamepad.start", "Start" },
            { "gamepad_button_select", "input.gamepad.select", "Back" },
            { "gamepad_trigger_l2", "input.gamepad.lt", "LT" },
            { "gamepad_trigger_r2", "input.gamepad.rt", "RT" },
            { "gamepad_thumbstick_l", "input.gamepad.left_stick", "Left Stick" },
            { "gamepad_thumbstick_r", "input.gamepad.right_stick", "Right Stick" },
            { "gamepad_thumbstick_l_x", "input.gamepad.left_stick_x", "Left Stick X" },
            { "gamepad_thumbstick_l_y", "input.gamepad.left_stick_y", "Left Stick Y" },
            { "gamepad_thumbstick_r_x", "input.gamepad.right_stick_x", "Right Stick X" },
            { "gamepad_thumbstick_r_y", "input.gamepad.right_stick_y", "Right Stick Y" },
            { "gamepad_thumbstick_l_up", "input.gamepad.left_stick_up", "Left Stick Up" },
            { "gamepad_thumbstick_l_down", "input.gamepad.left_stick_down", "Left Stick Down" },
            { "gamepad_thumbstick_l_left", "input.gamepad.left_stick_left", "Left Stick Left" },
            { "gamepad_thumbstick_l_right", "input.gamepad.left_stick_right", "Left Stick Right" },
            { "gamepad_thumbstick_r_up", "input.gamepad.right_stick_up", "Right Stick Up" },
            { "gamepad_thumbstick_r_down", "input.gamepad.right_stick_down", "Right Stick Down" },
            { "gamepad_thumbstick_r_left", "input.gamepad.right_stick_left", "Right Stick Left" },
            { "gamepad_thumbstick_r_right", "input.gamepad.right_stick_right", "Right Stick Right" },
        };

        constexpr AZStd::string_view AlphanumericPrefix = "keyboard_key_alphanumeric_";
        constexpr AZStd::string_view FunctionPrefix = "keyboard_key_function_F";
        constexpr AZStd::string_view NumpadPrefix = "keyboard_key_numpad_";

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

    AZStd::optional<AZStd::string> InputChannelText::GetDisplayText(AZStd::string_view channelName)
    {
        for (const ChannelText& entry : ChannelTexts)
        {
            if (entry.m_channelName == channelName)
            {
                return AZStd::string(entry.m_defaultText);
            }
        }

        // "keyboard_key_alphanumeric_E" -> "E", "keyboard_key_alphanumeric_7" -> "7"
        if (AZStd::string_view key = AfterPrefix(channelName, AlphanumericPrefix); !key.empty())
        {
            return AZStd::string(key);
        }

        // "keyboard_key_function_F01" -> "F1": the engine names have two digits, players expect no leading zero.
        if (AZStd::string_view number = AfterPrefix(channelName, FunctionPrefix); IsAllDigits(number))
        {
            while (number.size() > 1 && number.front() == '0')
            {
                number.remove_prefix(1);
            }
            return AZStd::string("F") + AZStd::string(number);
        }

        // "keyboard_key_numpad_5" -> "Num 5" (the other numpad keys are in the table)
        if (AZStd::string_view digit = AfterPrefix(channelName, NumpadPrefix); digit.size() == 1 && IsAllDigits(digit))
        {
            return AZStd::string("Num ") + AZStd::string(digit);
        }

        return AZStd::nullopt;
    }
} // namespace RebindableInput
