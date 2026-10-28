
#include <PlayerBindingsFile.h>

#include <InputChannelInfo.h>

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Serialization/Json/JsonSerializationSettings.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>

namespace RebindableInput
{
    namespace
    {
        bool IsSameSlot(const SlotRecord& a, const SlotRecord& b)
        {
            return a.m_actionName == b.m_actionName && a.m_group == b.m_group && a.m_slot == b.m_slot;
        }

        PlayerBindingsFileSlot ToFileSlot(const SlotRecord& record)
        {
            return PlayerBindingsFileSlot{
                record.m_actionName,
                PlayerBindingsFile::ToString(record.m_group),
                PlayerBindingsFile::ToString(record.m_slot),
                record.m_channelName };
        }

        //! Adds the slot of the file to the records, unless it cannot be used: then the reason goes to the warnings.
        void AddRecord(
            const PlayerBindingsFileSlot& fileSlot, bool isOverride, AZStd::vector<SlotRecord>& records, AZStd::vector<AZStd::string>& warnings)
        {
            const char* listName = isOverride ? "Overrides" : "Defaults";

            const AZStd::optional<DeviceGroup> group = PlayerBindingsFile::ParseDeviceGroup(fileSlot.m_deviceGroup);
            const AZStd::optional<BindingSlot> slot = PlayerBindingsFile::ParseSlot(fileSlot.m_slot);
            if (fileSlot.m_action.empty() || !group || !slot)
            {
                warnings.push_back(AZStd::string::format(
                    "%s: action '%s', device group '%s', slot '%s' is not a valid slot, left out.",
                    listName, fileSlot.m_action.c_str(), fileSlot.m_deviceGroup.c_str(), fileSlot.m_slot.c_str()));
                return;
            }

            SlotRecord record{ fileSlot.m_action, *group, *slot, fileSlot.m_channel };

            // Only the changes are checked: a default is only compared, never applied.
            if (isOverride && !record.m_channelName.empty())
            {
                const AZStd::optional<ChannelInfo> info = InputChannelInfo::Find(record.m_channelName);
                if (!info)
                {
                    warnings.push_back(AZStd::string::format(
                        "%s: action '%s': '%s' is not an input channel of the engine, left out.",
                        listName, record.m_actionName.c_str(), record.m_channelName.c_str()));
                    return;
                }
                if (info->m_deviceGroup != record.m_group)
                {
                    warnings.push_back(AZStd::string::format(
                        "%s: action '%s': '%s' does not belong to the device group '%s', left out.",
                        listName, record.m_actionName.c_str(), record.m_channelName.c_str(), fileSlot.m_deviceGroup.c_str()));
                    return;
                }
            }

            const bool isDuplicate = AZStd::any_of(records.begin(), records.end(),
                [&record](const SlotRecord& other) { return IsSameSlot(other, record); });
            if (isDuplicate)
            {
                warnings.push_back(AZStd::string::format(
                    "%s: action '%s', %s %s is in the file twice, the first one is used.",
                    listName, record.m_actionName.c_str(), fileSlot.m_deviceGroup.c_str(), fileSlot.m_slot.c_str()));
                return;
            }

            records.push_back(AZStd::move(record));
        }
    } // namespace

    void PlayerBindingsFile::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            // The field names are the keys in the JSON file.
            serializeContext->Class<PlayerBindingsFileSlot>()
                ->Version(1)
                ->Field("Action", &PlayerBindingsFileSlot::m_action)
                ->Field("DeviceGroup", &PlayerBindingsFileSlot::m_deviceGroup)
                ->Field("Slot", &PlayerBindingsFileSlot::m_slot)
                ->Field("Channel", &PlayerBindingsFileSlot::m_channel)
                ;

            serializeContext->Class<PlayerBindingsFileContents>()
                ->Version(1)
                ->Field("Version", &PlayerBindingsFileContents::m_version)
                ->Field("Overrides", &PlayerBindingsFileContents::m_overrides)
                ->Field("Defaults", &PlayerBindingsFileContents::m_defaults)
                ;
        }
    }

    AZ::Outcome<AZStd::string, AZStd::string> PlayerBindingsFile::ToJson(const PlayerBindingsData& data)
    {
        const PlayerBindingsFileContents contents = ToContents(data);

        // Every field is written, also an empty channel (an emptied slot) and the version:
        // without this, values equal to the defaults are left out of the file.
        AZ::JsonSerializerSettings settings;
        settings.m_keepDefaults = true;

        AZStd::string text;
        AZ::IO::ByteContainerStream<AZStd::string> stream(&text);
        // The template argument is explicit: ObjectType cannot be deduced from the nullptr of defaultClassPtr.
        const AZ::Outcome<void, AZStd::string> saved =
            AZ::JsonSerializationUtils::SaveObjectToStream<PlayerBindingsFileContents>(&contents, stream, nullptr, &settings);
        if (!saved.IsSuccess())
        {
            return AZ::Failure(saved.GetError());
        }
        return AZ::Success(AZStd::move(text));
    }

    AZ::Outcome<PlayerBindingsData, AZStd::string> PlayerBindingsFile::FromJson(
        AZStd::string_view text, AZStd::vector<AZStd::string>& warnings)
    {
        PlayerBindingsFileContents contents;
        const AZ::Outcome<void, AZStd::string> loaded = AZ::JsonSerializationUtils::LoadObjectFromString(contents, text);
        if (!loaded.IsSuccess())
        {
            return AZ::Failure(loaded.GetError());
        }
        if (contents.m_version > PlayerBindingsFileContents::CurrentVersion)
        {
            warnings.push_back(AZStd::string::format(
                "The file has version %u, this gem knows version %u: what this version knows is used.",
                contents.m_version, PlayerBindingsFileContents::CurrentVersion));
        }
        return AZ::Success(FromContents(contents, warnings));
    }

    PlayerBindingsFileContents PlayerBindingsFile::ToContents(const PlayerBindingsData& data)
    {
        PlayerBindingsFileContents contents;
        for (const SlotRecord& record : data.m_overrides)
        {
            contents.m_overrides.push_back(ToFileSlot(record));
        }
        for (const SlotRecord& record : data.m_defaults)
        {
            contents.m_defaults.push_back(ToFileSlot(record));
        }
        return contents;
    }

    PlayerBindingsData PlayerBindingsFile::FromContents(const PlayerBindingsFileContents& contents, AZStd::vector<AZStd::string>& warnings)
    {
        PlayerBindingsData data;
        for (const PlayerBindingsFileSlot& fileSlot : contents.m_overrides)
        {
            AddRecord(fileSlot, true, data.m_overrides, warnings);
        }
        for (const PlayerBindingsFileSlot& fileSlot : contents.m_defaults)
        {
            AddRecord(fileSlot, false, data.m_defaults, warnings);
        }
        return data;
    }

    const char* PlayerBindingsFile::ToString(DeviceGroup group)
    {
        return group == DeviceGroup::Gamepad ? "Gamepad" : "KeyboardMouse";
    }

    const char* PlayerBindingsFile::ToString(BindingSlot slot)
    {
        return slot == BindingSlot::Alternate ? "Alternate" : "Primary";
    }

    AZStd::optional<DeviceGroup> PlayerBindingsFile::ParseDeviceGroup(AZStd::string_view text)
    {
        if (text == "KeyboardMouse")
        {
            return DeviceGroup::KeyboardMouse;
        }
        if (text == "Gamepad")
        {
            return DeviceGroup::Gamepad;
        }
        return AZStd::nullopt;
    }

    AZStd::optional<BindingSlot> PlayerBindingsFile::ParseSlot(AZStd::string_view text)
    {
        if (text == "Primary")
        {
            return BindingSlot::Primary;
        }
        if (text == "Alternate")
        {
            return BindingSlot::Alternate;
        }
        return AZStd::nullopt;
    }

} // namespace RebindableInput
