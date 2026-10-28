
#include <PlayerBindingsStorage.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Settings/SettingsRegistry.h>
#include <AzCore/Utils/Utils.h>
#include <SaveData/SaveDataRequestBus.h>

namespace RebindableInput
{
    namespace
    {
        enum class StorageKind
        {
            ProjectUser,
            SaveData,
        };

        constexpr AZStd::string_view ProjectUserName = "ProjectUser";
        constexpr AZStd::string_view SaveDataName = "SaveData";

        //! The setting, or the default of this build; ProjectUser where SaveData does not run.
        //! warn: report a wrong setting and the fallback (on loading and saving, not for messages).
        StorageKind GetStorageKind([[maybe_unused]] bool warn)
        {
        #if defined(AZ_RELEASE_BUILD)
            StorageKind kind = StorageKind::SaveData;
        #else
            StorageKind kind = StorageKind::ProjectUser;
        #endif

            if (AZ::SettingsRegistryInterface* registry = AZ::SettingsRegistry::Get())
            {
                AZ::SettingsRegistryInterface::FixedValueString value;
                if (registry->Get(value, PlayerBindingsStorage::StorageSettingKey))
                {
                    if (AZStd::string_view(value) == ProjectUserName)
                    {
                        kind = StorageKind::ProjectUser;
                    }
                    else if (AZStd::string_view(value) == SaveDataName)
                    {
                        kind = StorageKind::SaveData;
                    }
                    else
                    {
                        AZ_Warning("RebindableInput", !warn, "Setting '%s' is '%s', expected 'ProjectUser' or 'SaveData': the default is used.",
                            PlayerBindingsStorage::StorageSettingKey, value.c_str());
                    }
                }
            }

            if (kind == StorageKind::SaveData && !SaveData::SaveDataRequestBus::HasHandlers())
            {
                AZ_Warning("RebindableInput", !warn,
                    "The SaveData gem is not active here (it runs only in game launchers, not in the Editor): "
                    "the player's bindings are stored in the project's user folder instead.");
                kind = StorageKind::ProjectUser;
            }
            return kind;
        }

        AZ::IO::FixedMaxPath GetProjectUserFilePath()
        {
            AZ::IO::FixedMaxPath path(AZ::Utils::GetProjectUserPath());
            path /= "RebindableInput";
            path /= PlayerBindingsStorage::FileName;
            return path;
        }
    } // namespace

    void PlayerBindingsStorage::Load(LoadCallback callback)
    {
        if (GetStorageKind(true) == StorageKind::SaveData)
        {
            SaveData::SaveDataRequests::LoadDataBufferParams params;
            params.dataBufferName = FileName;
            params.localUserId = AzFramework::LocalUserIdNone; // one file for the machine, not per local user
            params.callback = [callback](const SaveData::SaveDataNotifications::DataBufferLoadedParams& loaded)
            {
                using Result = SaveData::SaveDataNotifications::Result;
                if (loaded.result == Result::ErrorNotFound)
                {
                    callback(LoadResult::NotFound, AZStd::string());
                    return;
                }
                if (loaded.result != Result::Success || !loaded.dataBuffer)
                {
                    callback(LoadResult::Failed, AZStd::string());
                    return;
                }
                AZStd::string text(static_cast<const char*>(loaded.dataBuffer.get()), aznumeric_cast<size_t>(loaded.dataBufferSize));
                callback(LoadResult::Loaded, AZStd::move(text));
            };
            SaveData::SaveDataRequestBus::Broadcast(&SaveData::SaveDataRequests::LoadDataBuffer, params);
            return;
        }

        const AZ::IO::FixedMaxPath path = GetProjectUserFilePath();
        if (!AZ::IO::SystemFile::Exists(path.c_str()))
        {
            callback(LoadResult::NotFound, AZStd::string());
            return;
        }
        AZ::Outcome<AZStd::string, AZStd::string> read = AZ::Utils::ReadFile(path.Native());
        if (!read.IsSuccess())
        {
            AZ_Warning("RebindableInput", false, "Cannot read '%s': %s", path.c_str(), read.GetError().c_str());
            callback(LoadResult::Failed, AZStd::string());
            return;
        }
        callback(LoadResult::Loaded, read.TakeValue());
    }

    void PlayerBindingsStorage::Save(AZStd::string text, SaveCallback callback)
    {
        if (GetStorageKind(true) == StorageKind::SaveData)
        {
            using SaveDataRequests = SaveData::SaveDataRequests;
            SaveDataRequests::SaveDataBufferParams params;
            // SaveData frees the buffer with azfree when it is done, so it must come from azmalloc.
            params.dataBuffer = SaveDataRequests::DataBuffer(azmalloc(text.size()), SaveDataRequests::DataBufferDeleterAzFree);
            memcpy(params.dataBuffer.get(), text.data(), text.size());
            params.dataBufferSize = text.size();
            params.dataBufferName = FileName;
            params.localUserId = AzFramework::LocalUserIdNone;
            params.callback = [callback](const SaveData::SaveDataNotifications::DataBufferSavedParams& saved)
            {
                callback(saved.result == SaveData::SaveDataNotifications::Result::Success);
            };
            SaveData::SaveDataRequestBus::Broadcast(&SaveDataRequests::SaveDataBuffer, params);
            return;
        }

        // Written to a temporary file first and then renamed, like MultiplayerSample's user settings:
        // if the game stops in the middle of writing, the old file stays whole.
        const AZ::IO::FixedMaxPath path = GetProjectUserFilePath();
        AZ::IO::FixedMaxPath temporaryPath = path;
        temporaryPath.ReplaceExtension("json.tmp");

        const AZ::Outcome<void, AZStd::string> written = AZ::Utils::WriteFile(text, temporaryPath.Native());
        if (!written.IsSuccess())
        {
            AZ_Warning("RebindableInput", false, "Cannot write '%s': %s", temporaryPath.c_str(), written.GetError().c_str());
            callback(false);
            return;
        }
        const bool renamed = AZ::IO::SystemFile::Rename(temporaryPath.c_str(), path.c_str(), true);
        AZ_Warning("RebindableInput", renamed, "Cannot rename '%s' to '%s'.", temporaryPath.c_str(), path.c_str());
        callback(renamed);
    }

    AZStd::string PlayerBindingsStorage::GetLocation()
    {
        if (GetStorageKind(false) == StorageKind::SaveData)
        {
            return AZStd::string::format("SaveData gem, '%s'", FileName);
        }
        return AZStd::string(GetProjectUserFilePath().c_str());
    }

} // namespace RebindableInput
