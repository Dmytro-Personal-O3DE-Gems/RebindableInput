
#include <PlayerBindingsFile.h>

#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace RebindableInputTests
{
    using RebindableInput::BindingSlot;
    using RebindableInput::DeviceGroup;
    using RebindableInput::PlayerBindingsData;
    using RebindableInput::PlayerBindingsFile;
    using RebindableInput::PlayerBindingsFileContents;
    using RebindableInput::PlayerBindingsFileSlot;

    class PlayerBindingsFileTest : public UnitTest::LeakDetectionFixture
    {
    protected:
        static PlayerBindingsFileSlot FileSlot(const char* action, const char* group, const char* slot, const char* channel)
        {
            return PlayerBindingsFileSlot{ action, group, slot, channel };
        }
    };

    TEST_F(PlayerBindingsFileTest, ContentsRoundTripKeepsEveryRecord)
    {
        PlayerBindingsData data;
        data.m_overrides.push_back({ "Interact", DeviceGroup::KeyboardMouse, BindingSlot::Primary, "keyboard_key_alphanumeric_F" });
        data.m_overrides.push_back({ "Jump", DeviceGroup::Gamepad, BindingSlot::Alternate, "" });
        data.m_defaults.push_back({ "Interact", DeviceGroup::KeyboardMouse, BindingSlot::Primary, "keyboard_key_alphanumeric_E" });

        AZStd::vector<AZStd::string> warnings;
        const PlayerBindingsData loaded = PlayerBindingsFile::FromContents(PlayerBindingsFile::ToContents(data), warnings);

        EXPECT_TRUE(warnings.empty());
        ASSERT_EQ(loaded.m_overrides.size(), 2u);
        EXPECT_EQ(loaded.m_overrides[0].m_actionName, "Interact");
        EXPECT_EQ(loaded.m_overrides[0].m_group, DeviceGroup::KeyboardMouse);
        EXPECT_EQ(loaded.m_overrides[0].m_slot, BindingSlot::Primary);
        EXPECT_EQ(loaded.m_overrides[0].m_channelName, "keyboard_key_alphanumeric_F");
        EXPECT_EQ(loaded.m_overrides[1].m_group, DeviceGroup::Gamepad);
        EXPECT_EQ(loaded.m_overrides[1].m_slot, BindingSlot::Alternate);
        ASSERT_EQ(loaded.m_defaults.size(), 1u);
        EXPECT_EQ(loaded.m_defaults[0].m_channelName, "keyboard_key_alphanumeric_E");
    }

    TEST_F(PlayerBindingsFileTest, GroupsAndSlotsAreWrittenByName)
    {
        PlayerBindingsData data;
        data.m_overrides.push_back({ "Jump", DeviceGroup::Gamepad, BindingSlot::Alternate, "gamepad_button_a" });

        const PlayerBindingsFileContents contents = PlayerBindingsFile::ToContents(data);

        ASSERT_EQ(contents.m_overrides.size(), 1u);
        EXPECT_EQ(contents.m_overrides[0].m_deviceGroup, "Gamepad");
        EXPECT_EQ(contents.m_overrides[0].m_slot, "Alternate");
        EXPECT_EQ(contents.m_version, PlayerBindingsFileContents::CurrentVersion);
    }

    TEST_F(PlayerBindingsFileTest, EmptiedSlotIsKept)
    {
        PlayerBindingsFileContents contents;
        contents.m_overrides.push_back(FileSlot("Interact", "KeyboardMouse", "Primary", ""));

        AZStd::vector<AZStd::string> warnings;
        const PlayerBindingsData data = PlayerBindingsFile::FromContents(contents, warnings);

        EXPECT_TRUE(warnings.empty());
        ASSERT_EQ(data.m_overrides.size(), 1u);
        EXPECT_TRUE(data.m_overrides[0].m_channelName.empty());
    }

    TEST_F(PlayerBindingsFileTest, UnknownChannelIsLeftOut)
    {
        PlayerBindingsFileContents contents;
        contents.m_overrides.push_back(FileSlot("Interact", "KeyboardMouse", "Primary", "keyboard_key_alphanumeric_NOPE"));

        AZStd::vector<AZStd::string> warnings;
        const PlayerBindingsData data = PlayerBindingsFile::FromContents(contents, warnings);

        EXPECT_TRUE(data.m_overrides.empty());
        EXPECT_EQ(warnings.size(), 1u);
    }

    TEST_F(PlayerBindingsFileTest, ChannelOfTheOtherDeviceGroupIsLeftOut)
    {
        PlayerBindingsFileContents contents;
        contents.m_overrides.push_back(FileSlot("Interact", "KeyboardMouse", "Primary", "gamepad_button_a"));

        AZStd::vector<AZStd::string> warnings;
        const PlayerBindingsData data = PlayerBindingsFile::FromContents(contents, warnings);

        EXPECT_TRUE(data.m_overrides.empty());
        EXPECT_EQ(warnings.size(), 1u);
    }

    TEST_F(PlayerBindingsFileTest, UnknownGroupOrSlotNameIsLeftOut)
    {
        PlayerBindingsFileContents contents;
        contents.m_overrides.push_back(FileSlot("Interact", "Keyboard", "Primary", "keyboard_key_alphanumeric_F"));
        contents.m_overrides.push_back(FileSlot("Interact", "KeyboardMouse", "Third", "keyboard_key_alphanumeric_F"));
        contents.m_defaults.push_back(FileSlot("", "KeyboardMouse", "Primary", "keyboard_key_alphanumeric_E"));

        AZStd::vector<AZStd::string> warnings;
        const PlayerBindingsData data = PlayerBindingsFile::FromContents(contents, warnings);

        EXPECT_TRUE(data.m_overrides.empty());
        EXPECT_TRUE(data.m_defaults.empty());
        EXPECT_EQ(warnings.size(), 3u);
    }

    TEST_F(PlayerBindingsFileTest, SecondRecordForTheSameSlotIsLeftOut)
    {
        PlayerBindingsFileContents contents;
        contents.m_overrides.push_back(FileSlot("Interact", "KeyboardMouse", "Primary", "keyboard_key_alphanumeric_F"));
        contents.m_overrides.push_back(FileSlot("Interact", "KeyboardMouse", "Primary", "keyboard_key_alphanumeric_G"));

        AZStd::vector<AZStd::string> warnings;
        const PlayerBindingsData data = PlayerBindingsFile::FromContents(contents, warnings);

        ASSERT_EQ(data.m_overrides.size(), 1u);
        EXPECT_EQ(data.m_overrides[0].m_channelName, "keyboard_key_alphanumeric_F");
        EXPECT_EQ(warnings.size(), 1u);
    }

    TEST_F(PlayerBindingsFileTest, UnknownActionIsKept)
    {
        // An action is known only while an entity with it is active: it may be on another level.
        PlayerBindingsFileContents contents;
        contents.m_overrides.push_back(FileSlot("OpenMapOnLevel3", "KeyboardMouse", "Primary", "keyboard_key_alphanumeric_M"));

        AZStd::vector<AZStd::string> warnings;
        const PlayerBindingsData data = PlayerBindingsFile::FromContents(contents, warnings);

        EXPECT_TRUE(warnings.empty());
        EXPECT_EQ(data.m_overrides.size(), 1u);
    }

    TEST_F(PlayerBindingsFileTest, NamesAreParsedExactly)
    {
        EXPECT_EQ(PlayerBindingsFile::ParseDeviceGroup("Gamepad"), DeviceGroup::Gamepad);
        EXPECT_EQ(PlayerBindingsFile::ParseSlot("Alternate"), BindingSlot::Alternate);
        EXPECT_FALSE(PlayerBindingsFile::ParseDeviceGroup("gamepad").has_value());
        EXPECT_FALSE(PlayerBindingsFile::ParseSlot("primary").has_value());
    }
} // namespace RebindableInputTests
