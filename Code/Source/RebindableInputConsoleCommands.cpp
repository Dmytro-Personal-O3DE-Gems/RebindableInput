
#include "RebindableInputConsoleCommands.h"

#include <RebindableInput/RebindableInputBus.h>

#include <AzCore/Console/ConsoleTypeHelpers.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/string/string.h>

namespace RebindableInput
{
    namespace
    {
        constexpr const char* Window = "RebindableInput";

        // Local commands: never sent to other machines in multiplayer.
        constexpr AZ::ConsoleFunctorFlags CommandFlags = AZ::ConsoleFunctorFlags::DontReplicate;

        constexpr const char* SetUsage =
            "rebind_set <action> <KeyboardMouse|km|Gamepad|gp> <Primary|p|Alternate|a> <channel> [Move|Duplicate]";
        constexpr const char* ClearUsage = "rebind_clear <action> <KeyboardMouse|km|Gamepad|gp> <Primary|p|Alternate|a>";

        // Case-insensitive.
        bool Matches(AZStd::string_view text, AZStd::string_view fullName, AZStd::string_view shortName)
        {
            return AZ::StringFunc::Equal(text, fullName) || AZ::StringFunc::Equal(text, shortName);
        }

        AZStd::optional<DeviceGroup> ParseDeviceGroup(AZStd::string_view text)
        {
            if (Matches(text, "KeyboardMouse", "km"))
            {
                return DeviceGroup::KeyboardMouse;
            }
            if (Matches(text, "Gamepad", "gp"))
            {
                return DeviceGroup::Gamepad;
            }
            return AZStd::nullopt;
        }

        AZStd::optional<BindingSlot> ParseSlot(AZStd::string_view text)
        {
            if (Matches(text, "Primary", "p"))
            {
                return BindingSlot::Primary;
            }
            if (Matches(text, "Alternate", "a"))
            {
                return BindingSlot::Alternate;
            }
            return AZStd::nullopt;
        }

        AZStd::optional<ConflictMode> ParseConflictMode(AZStd::string_view text)
        {
            if (Matches(text, "Move", "m"))
            {
                return ConflictMode::Move;
            }
            if (Matches(text, "Duplicate", "AllowDuplicate"))
            {
                return ConflictMode::AllowDuplicate;
            }
            return AZStd::nullopt;
        }

        const char* ToString(BindingSlot slot)
        {
            return slot == BindingSlot::Primary ? "Primary" : "Alternate";
        }

        const char* ToString(SetBindingResult result)
        {
            switch (result)
            {
            case SetBindingResult::Ok:
                return "Ok";
            case SetBindingResult::UnknownAction:
                return "unknown action (no active bindings have it)";
            case SetBindingResult::UnknownChannel:
                return "unknown channel (not a keyboard, mouse or gamepad channel of the engine)";
            case SetBindingResult::WrongDeviceGroup:
                return "the channel belongs to the other device group";
            case SetBindingResult::WrongChannelKind:
                return "the channel is of another kind than the action expects (button, mouse axis or stick)";
            case SetBindingResult::ReservedChannel:
                return "Escape and gamepad Start are reserved: they cancel a capture";
            }
            return "unknown result";
        }

        //! The rebinding API, or nullptr with a message.
        RebindableInputRequests* GetApi()
        {
            RebindableInputRequests* api = RebindableInputInterface::Get();
            if (!api)
            {
                AZ_Printf(Window, "The rebinding API is not available: RebindableInputSystemComponent is not registered.\n");
            }
            return api;
        }

        bool IsKnownAction(const RebindableInputRequests& api, const AZStd::string& actionName)
        {
            const AZStd::vector<AZStd::string> actions = api.GetActions();
            return AZStd::find(actions.begin(), actions.end(), actionName) != actions.end();
        }

        //! "E (keyboard_key_alphanumeric_E)" or "-" for an empty slot.
        AZStd::string SlotToString(const RebindableInputRequests& api, const AZStd::string& actionName, DeviceGroup group, BindingSlot slot)
        {
            const AZStd::optional<SlotBinding> binding = api.GetBinding(actionName, group, slot);
            if (!binding)
            {
                return "-";
            }
            return AZStd::string::format("%s (%s)", binding->m_displayText.c_str(), binding->m_channelName.c_str());
        }

        //! One line per action: both slots of both device groups.
        void PrintAction(const RebindableInputRequests& api, const AZStd::string& actionName)
        {
            AZ_Printf(Window, "%-20s KM: %s | %s    GP: %s | %s\n",
                actionName.c_str(),
                SlotToString(api, actionName, DeviceGroup::KeyboardMouse, BindingSlot::Primary).c_str(),
                SlotToString(api, actionName, DeviceGroup::KeyboardMouse, BindingSlot::Alternate).c_str(),
                SlotToString(api, actionName, DeviceGroup::Gamepad, BindingSlot::Primary).c_str(),
                SlotToString(api, actionName, DeviceGroup::Gamepad, BindingSlot::Alternate).c_str());
        }

        //! "<action> <group> <slot>": the first three arguments of rebind_set and rebind_clear.
        struct SlotAddress
        {
            AZStd::string m_actionName;
            DeviceGroup m_group = DeviceGroup::KeyboardMouse;
            BindingSlot m_slot = BindingSlot::Primary;
        };

        //! Parses the slot address, or prints what is wrong and returns an empty optional.
        AZStd::optional<SlotAddress> ParseSlotAddress(const AZ::ConsoleCommandContainer& arguments, const char* usage)
        {
            const AZStd::optional<DeviceGroup> group = ParseDeviceGroup(arguments[1]);
            if (!group)
            {
                AZ_Printf(Window, "Unknown device group '%.*s'. Usage: %s\n", AZ_STRING_ARG(arguments[1]), usage);
                return AZStd::nullopt;
            }
            const AZStd::optional<BindingSlot> slot = ParseSlot(arguments[2]);
            if (!slot)
            {
                AZ_Printf(Window, "Unknown slot '%.*s'. Usage: %s\n", AZ_STRING_ARG(arguments[2]), usage);
                return AZStd::nullopt;
            }
            return SlotAddress{ AZStd::string(arguments[0]), *group, *slot };
        }

        void RebindList(const AZ::ConsoleCommandContainer& arguments)
        {
            const RebindableInputRequests* api = GetApi();
            if (!api)
            {
                return;
            }

            if (!arguments.empty())
            {
                const AZStd::string actionName(arguments[0]);
                if (!IsKnownAction(*api, actionName))
                {
                    AZ_Printf(Window, "Unknown action '%s'.\n", actionName.c_str());
                    return;
                }
                PrintAction(*api, actionName);
                return;
            }

            const AZStd::vector<AZStd::string> actions = api->GetActions();
            if (actions.empty())
            {
                AZ_Printf(Window, "No active bindings: no entity with a Rebindable Input component is active.\n");
                return;
            }
            for (const AZStd::string& actionName : actions)
            {
                PrintAction(*api, actionName);
            }
        }

        void RebindSet(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() != 4 && arguments.size() != 5)
            {
                AZ_Printf(Window, "Usage: %s\n", SetUsage);
                return;
            }
            RebindableInputRequests* api = GetApi();
            if (!api)
            {
                return;
            }
            const AZStd::optional<SlotAddress> address = ParseSlotAddress(arguments, SetUsage);
            if (!address)
            {
                return;
            }
            ConflictMode mode = ConflictMode::Move;
            if (arguments.size() == 5)
            {
                const AZStd::optional<ConflictMode> parsedMode = ParseConflictMode(arguments[4]);
                if (!parsedMode)
                {
                    AZ_Printf(Window, "Unknown conflict mode '%.*s'. Usage: %s\n", AZ_STRING_ARG(arguments[4]), SetUsage);
                    return;
                }
                mode = *parsedMode;
            }

            const AZStd::string channelName(arguments[3]);

            // Asked before SetBinding, the way a menu would: after a Move the conflict is gone.
            const AZStd::optional<BindingConflict> conflict =
                api->FindConflict(address->m_actionName, address->m_group, address->m_slot, channelName);

            const SetBindingResult result = api->SetBinding(address->m_actionName, address->m_group, address->m_slot, channelName, mode);
            if (result != SetBindingResult::Ok)
            {
                AZ_Printf(Window, "Not changed: %s.\n", ToString(result));
                return;
            }

            if (conflict)
            {
                AZ_Printf(Window, "%s: %s %s (%s).\n",
                    mode == ConflictMode::Move ? "Moved from" : "Also used by",
                    conflict->m_actionName.c_str(), ToString(conflict->m_slot),
                    mode == ConflictMode::Move ? "that slot is empty now" : "one key triggers both");
                PrintAction(*api, conflict->m_actionName);
            }
            if (!conflict || conflict->m_actionName != address->m_actionName)
            {
                PrintAction(*api, address->m_actionName);
            }
        }

        void RebindClear(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() != 3)
            {
                AZ_Printf(Window, "Usage: %s\n", ClearUsage);
                return;
            }
            RebindableInputRequests* api = GetApi();
            if (!api)
            {
                return;
            }
            const AZStd::optional<SlotAddress> address = ParseSlotAddress(arguments, ClearUsage);
            if (!address)
            {
                return;
            }
            // ClearBinding ignores unknown actions silently; a typo deserves a message here.
            if (!IsKnownAction(*api, address->m_actionName))
            {
                AZ_Printf(Window, "Unknown action '%s'.\n", address->m_actionName.c_str());
                return;
            }

            api->ClearBinding(address->m_actionName, address->m_group, address->m_slot);
            PrintAction(*api, address->m_actionName);
        }

        void RebindReset(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() > 1)
            {
                AZ_Printf(Window, "Usage: rebind_reset [action]\n");
                return;
            }
            RebindableInputRequests* api = GetApi();
            if (!api)
            {
                return;
            }

            if (arguments.empty())
            {
                api->ResetAll();
                AZ_Printf(Window, "All actions are back to the defaults.\n");
                return;
            }

            const AZStd::string actionName(arguments[0]);
            if (!IsKnownAction(*api, actionName))
            {
                AZ_Printf(Window, "Unknown action '%s'.\n", actionName.c_str());
                return;
            }
            api->ResetAction(actionName);
            PrintAction(*api, actionName);
        }

        void RebindSave(const AZ::ConsoleCommandContainer& arguments)
        {
            if (!arguments.empty())
            {
                AZ_Printf(Window, "Usage: rebind_save\n");
                return;
            }
            if (RebindableInputRequests* api = GetApi())
            {
                // The result is logged by the system component when the storage answers.
                api->SaveBindings();
            }
        }

        void RebindRevert(const AZ::ConsoleCommandContainer& arguments)
        {
            if (!arguments.empty())
            {
                AZ_Printf(Window, "Usage: rebind_revert\n");
                return;
            }
            if (RebindableInputRequests* api = GetApi())
            {
                api->RevertBindings();
                AZ_Printf(Window, "Back to the last saved or loaded bindings.\n");
                RebindList(AZ::ConsoleCommandContainer{});
            }
        }

        RebindableInputConsoleCommands* s_instance = nullptr;

        constexpr const char* CaptureUsage =
            "rebind_capture <action> <KeyboardMouse|km|Gamepad|gp> <Primary|p|Alternate|a> [delay seconds, 0 by default]";

        //! ImGui's default key to show and hide its console (ImGuiManager.cpp).
        constexpr AZStd::string_view ImGuiConsoleKey = "keyboard_key_navigation_home";

        void RebindCapture(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() != 3 && arguments.size() != 4)
            {
                AZ_Printf(Window, "Usage: %s\n", CaptureUsage);
                return;
            }
            const AZStd::optional<SlotAddress> address = ParseSlotAddress(arguments, CaptureUsage);
            if (!address)
            {
                return;
            }
            float delaySeconds = 0.0f;
            if (arguments.size() == 4 && !AZ::ConsoleTypeHelpers::StringToValue(delaySeconds, arguments[3]))
            {
                AZ_Printf(Window, "'%.*s' is not a number. Usage: %s\n", AZ_STRING_ARG(arguments[3]), CaptureUsage);
                return;
            }
            if (RebindableInputConsoleCommands* commands = RebindableInputConsoleCommands::GetInstance())
            {
                commands->ScheduleCapture(address->m_actionName, address->m_group, address->m_slot, delaySeconds);
                AZ_Printf(Window, "Capture for '%s': close ImGui with Home, then press the key. Escape cancels "
                    "(in the Editor, Escape leaves Game Mode first).\n", address->m_actionName.c_str());
            }
        }
    } // namespace

    RebindableInputConsoleCommands::RebindableInputConsoleCommands()
        : m_list("rebind_list", "Prints the key slots of every input action, or of one: rebind_list [action]",
            CommandFlags, AZ::TypeId::CreateNull(), &RebindList)
        , m_set("rebind_set", "Puts a key into a slot of an input action. Usage: " "rebind_set <action> <KeyboardMouse|km|Gamepad|gp> "
            "<Primary|p|Alternate|a> <channel> [Move|Duplicate]",
            CommandFlags, AZ::TypeId::CreateNull(), &RebindSet)
        , m_clear("rebind_clear", "Empties a slot of an input action. Usage: rebind_clear <action> <group> <slot>",
            CommandFlags, AZ::TypeId::CreateNull(), &RebindClear)
        , m_reset("rebind_reset", "Returns one input action or all of them to the defaults: rebind_reset [action]",
            CommandFlags, AZ::TypeId::CreateNull(), &RebindReset)
        , m_save("rebind_save", "Writes the player's changes to the player's bindings file",
            CommandFlags, AZ::TypeId::CreateNull(), &RebindSave)
        , m_revert("rebind_revert", "Returns to the player's changes of the last save or load",
            CommandFlags, AZ::TypeId::CreateNull(), &RebindRevert)
        , m_capture("rebind_capture", "Waits for the next key and binds it to a slot. Usage: rebind_capture <action> <group> <slot> [delay]",
            CommandFlags, AZ::TypeId::CreateNull(), &RebindCapture)
    {
        s_instance = this;
        RebindableInputNotificationBus::Handler::BusConnect();
    }

    RebindableInputConsoleCommands::~RebindableInputConsoleCommands()
    {
        // The bus handlers disconnect in their own destructors.
        s_instance = nullptr;
    }

    RebindableInputConsoleCommands* RebindableInputConsoleCommands::GetInstance()
    {
        return s_instance;
    }

    void RebindableInputConsoleCommands::ScheduleCapture(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, float delaySeconds)
    {
        m_pendingCapture = PendingCapture{ actionName, group, slot };
        m_secondsLeft = delaySeconds;
        AZ::TickBus::Handler::BusConnect();
    }

    void RebindableInputConsoleCommands::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_secondsLeft -= deltaTime;
        if (m_secondsLeft > 0.0f || !m_pendingCapture)
        {
            return;
        }
        AZ::TickBus::Handler::BusDisconnect();
        const PendingCapture pending = *m_pendingCapture;
        m_pendingCapture.reset();

        RebindableInputRequests* api = GetApi();
        if (!api)
        {
            return;
        }
        if (!api->StartCapture(pending.m_actionName, pending.m_group, pending.m_slot))
        {
            AZ_Printf(Window, "Cannot capture for '%s': unknown action, or the slot accepts no channel.\n", pending.m_actionName.c_str());
            return;
        }
        // Set after StartCapture: it may cancel an older capture of this console, whose notification comes during the call.
        m_captureStartedHere = true;
        AZ_Printf(Window, "Press a key for '%s' (%s %s). Escape cancels.\n", pending.m_actionName.c_str(),
            pending.m_group == DeviceGroup::Gamepad ? "Gamepad" : "KeyboardMouse", ToString(pending.m_slot));
    }

    void RebindableInputConsoleCommands::OnChannelCaptured(
        const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, const AZStd::string& channelName)
    {
        if (!m_captureStartedHere)
        {
            return; // a capture of a menu, not of this console
        }
        m_captureStartedHere = false;

        RebindableInputRequests* api = GetApi();
        if (!api)
        {
            return;
        }

        if (channelName == ImGuiConsoleKey)
        {
            // Home hid ImGui (ImGui lets it through): not the player's choice, wait for the next key.
            // The held Home stays swallowed until it is released, and the new capture starts with the next frame.
            m_captureStartedHere = api->StartCapture(actionName, group, slot);
            return;
        }

        AZ_Printf(Window, "Captured '%s'.\n", channelName.c_str());

        // What a settings menu does after the key arrives: ask about a conflict (here: always move), then bind.
        const AZStd::optional<BindingConflict> conflict = api->FindConflict(actionName, group, slot, channelName);
        const SetBindingResult result = api->SetBinding(actionName, group, slot, channelName, ConflictMode::Move);
        if (result != SetBindingResult::Ok)
        {
            AZ_Printf(Window, "Not changed: %s.\n", ToString(result));
            return;
        }
        if (conflict)
        {
            AZ_Printf(Window, "Moved from %s %s (that slot is empty now).\n", conflict->m_actionName.c_str(), ToString(conflict->m_slot));
            PrintAction(*api, conflict->m_actionName);
        }
        if (!conflict || conflict->m_actionName != actionName)
        {
            PrintAction(*api, actionName);
        }
    }

    void RebindableInputConsoleCommands::OnCaptureCancelled(
        const AZStd::string& actionName, [[maybe_unused]] DeviceGroup group, [[maybe_unused]] BindingSlot slot)
    {
        if (!m_captureStartedHere)
        {
            return;
        }
        m_captureStartedHere = false;
        AZ_Printf(Window, "Capture for '%s' cancelled.\n", actionName.c_str());
    }

} // namespace RebindableInput
