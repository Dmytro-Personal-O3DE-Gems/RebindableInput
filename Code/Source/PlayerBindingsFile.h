
#pragma once

#include <RebindableInput/RebindableInputBus.h>

#include <AzCore/Outcome/Outcome.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace AZ
{
    class ReflectContext;
}

namespace RebindableInput
{
    //! One slot of one input action: a change of the player, or a default as it was when the player saved.
    struct SlotRecord
    {
        AZStd::string m_actionName;
        DeviceGroup m_group = DeviceGroup::KeyboardMouse;
        BindingSlot m_slot = BindingSlot::Primary;
        AZStd::string m_channelName; //!< empty: an empty slot
    };

    //! What the player's file holds.
    struct PlayerBindingsData
    {
        //! The player's changes: slots that differ from the defaults.
        AZStd::vector<SlotRecord> m_overrides;

        //! The defaults of every slot the player has seen, as they were at the last save.
        //! On loading, a default that is new or changed since then (a game update) and uses a key the player took
        //! is emptied: the player's choice wins. A duplicate the player made on purpose (ConflictMode::AllowDuplicate)
        //! has an unchanged default, so it is kept.
        AZStd::vector<SlotRecord> m_defaults;
    };

    //! A slot as it is written to the file. Device groups and slots are stored by name ("KeyboardMouse", "Primary"):
    //! the file stays readable and does not depend on the values of the enums.
    struct PlayerBindingsFileSlot
    {
        AZ_TYPE_INFO(PlayerBindingsFileSlot, "{AB93C4D3-44B2-4EA6-9363-C4A60BF2490C}");

        AZStd::string m_action;
        AZStd::string m_deviceGroup;
        AZStd::string m_slot;
        AZStd::string m_channel;
    };

    //! The file as it is written: JSON through AZ::JsonSerializationUtils.
    struct PlayerBindingsFileContents
    {
        AZ_TYPE_INFO(PlayerBindingsFileContents, "{94506436-B2B2-436B-AC5F-D0DCA4E5193E}");

        static constexpr AZ::u32 CurrentVersion = 1;

        AZ::u32 m_version = CurrentVersion;
        AZStd::vector<PlayerBindingsFileSlot> m_overrides;
        AZStd::vector<PlayerBindingsFileSlot> m_defaults;
    };

    //! Converts the player's bindings to the file and back.
    class PlayerBindingsFile
    {
    public:
        static void Reflect(AZ::ReflectContext* context);

        //! JSON text of the data. Uses the serialize and JSON registration contexts of the application.
        static AZ::Outcome<AZStd::string, AZStd::string> ToJson(const PlayerBindingsData& data);

        //! The data from JSON text, or the reason it is not a bindings file.
        //! Records that cannot be used are left out and described in warnings (see FromContents).
        static AZ::Outcome<PlayerBindingsData, AZStd::string> FromJson(AZStd::string_view text, AZStd::vector<AZStd::string>& warnings);

        //! The conversion without JSON: it needs no engine, so it is unit tested.
        static PlayerBindingsFileContents ToContents(const PlayerBindingsData& data);

        //! Leaves out, with a warning: records without an action or with an unknown device group or slot name,
        //! a second record for the same slot, and changes to a channel that is not an engine channel or belongs to
        //! the other device group (the file may be edited by hand).
        //! Unknown actions are kept: an action is known only while an entity with it is active.
        static PlayerBindingsData FromContents(const PlayerBindingsFileContents& contents, AZStd::vector<AZStd::string>& warnings);

        static const char* ToString(DeviceGroup group);
        static const char* ToString(BindingSlot slot);

        //! Case-sensitive: the names are written by ToString.
        static AZStd::optional<DeviceGroup> ParseDeviceGroup(AZStd::string_view text);
        static AZStd::optional<BindingSlot> ParseSlot(AZStd::string_view text);
    };

} // namespace RebindableInput
