
#pragma once

#include <AzCore/std/functional.h>
#include <AzCore/std/string/string.h>

namespace RebindableInput
{
    //! Where the player's bindings file lives, and reading and writing it.
    //!
    //! Two places, chosen by the setting "/RebindableInput/Storage" in the Settings Registry:
    //! - "ProjectUser": <project>/user/RebindableInput/<FileName>. Shared by the Editor and the game launcher,
    //!   easy to open. Default in debug and profile builds.
    //! - "SaveData": the SaveData gem, the folder the OS has for user data (on Windows
    //!   %LOCALAPPDATA%\<executable name>\SaveData\). Default in release builds: the project folder of an
    //!   installed game may not be writable. SaveData runs only in game launchers, not in the Editor:
    //!   there "ProjectUser" is used instead, with a warning.
    //!
    //! The file content is the same in both places (JSON, see PlayerBindingsFile).
    class PlayerBindingsStorage
    {
    public:
        enum class LoadResult
        {
            Loaded,
            NotFound, //!< no file yet: the player has never saved
            Failed,
        };

        //! text is the file content when the result is Loaded, empty otherwise.
        using LoadCallback = AZStd::function<void(LoadResult result, AZStd::string text)>;
        using SaveCallback = AZStd::function<void(bool success)>;

        static constexpr const char* FileName = "RebindableInputBindings.json";
        static constexpr const char* StorageSettingKey = "/RebindableInput/Storage";

        //! The callback runs on the main thread: during the call (ProjectUser) or later (SaveData).
        static void Load(LoadCallback callback);

        //! The callback runs on the main thread: during the call (ProjectUser) or later (SaveData).
        static void Save(AZStd::string text, SaveCallback callback);

        //! Where the file is, for messages.
        static AZStd::string GetLocation();
    };

} // namespace RebindableInput
