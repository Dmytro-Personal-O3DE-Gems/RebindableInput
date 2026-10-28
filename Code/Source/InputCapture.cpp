
#include <InputCapture.h>

#include <RebindableInput/RebindableInputNotificationBus.h>

#include <AzCore/Component/TickBus.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/smart_ptr/weak_ptr.h>
#include <AzFramework/Input/Channels/InputChannel.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>

namespace RebindableInput
{
    namespace
    {
        //! Triggers and stick directions (0..1), stick axes (-1..1) and whole sticks (length 0..1).
        constexpr float AnalogThreshold = 0.5f;

        //! Mouse movement per event, in the raw units of the mouse. TODO: tune in the game.
        constexpr float MouseMovementThreshold = 10.0f;

        bool Contains(const AZStd::vector<AZStd::string>& channels, AZStd::string_view channelName)
        {
            return AZStd::find(channels.begin(), channels.end(), channelName) != channels.end();
        }

        void Remove(AZStd::vector<AZStd::string>& channels, AZStd::string_view channelName)
        {
            AZStd::erase_if(channels, [channelName](const AZStd::string& channel) { return channel == channelName; });
        }
    } // namespace

    InputCapture::InputCapture()
        // Above LyShine (GetPriorityUI) and the game (GetPriorityDefault), below ImGui (GetPriorityDebugUI),
        // so the debug console still works while capturing.
        : AzFramework::InputChannelEventListener(AzFramework::InputChannelEventListener::GetPriorityUI() + 1, false)
    {
    }

    bool InputCapture::IsReservedChannel(AZStd::string_view channelName)
    {
        return channelName == AzFramework::InputDeviceKeyboard::Key::Escape.GetName() ||
            channelName == AzFramework::InputDeviceGamepad::Button::Start.GetName();
    }

    void InputCapture::Start(const AZStd::string& actionName, DeviceGroup group, BindingSlot slot, ChannelKind kind)
    {
        Cancel();

        m_target = Target{ actionName, group, slot, kind };
        m_armed = false;
        m_pressedAfterArming.clear();

        const AZ::u32 generation = ++m_generation;
        AZStd::weak_ptr<int> lifetime = m_lifetime;
        AZ::TickBus::QueueFunction([this, lifetime, generation]()
        {
            // This object may be gone, or the capture cancelled or replaced, before the next tick.
            if (lifetime.expired() || generation != m_generation || !m_target)
            {
                return;
            }
            m_armed = true;
            ConnectToInput();
        });
    }

    void InputCapture::Cancel()
    {
        if (!m_target)
        {
            return;
        }
        const Target target = *m_target;
        StopWaiting();
        // Last: a handler may start the next capture.
        RebindableInputNotificationBus::Broadcast(
            &RebindableInputNotifications::OnCaptureCancelled, target.m_actionName, target.m_group, target.m_slot);
    }

    bool InputCapture::IsCapturing() const
    {
        return m_target.has_value();
    }

    bool InputCapture::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        const AZStd::string_view channelName = inputChannel.GetInputChannelId().GetName();

        if (Contains(m_swallowedUntilReleased, channelName))
        {
            if (inputChannel.IsStateEnded())
            {
                Remove(m_swallowedUntilReleased, channelName);
                DisconnectIfIdle();
            }
            return true;
        }

        if (!m_target || !m_armed)
        {
            return false;
        }

        if (inputChannel.IsStateBegan() && !Contains(m_pressedAfterArming, channelName))
        {
            m_pressedAfterArming.emplace_back(channelName);
        }
        else if (inputChannel.IsStateEnded())
        {
            Remove(m_pressedAfterArming, channelName);
        }

        if (IsReservedChannel(channelName) && inputChannel.IsStateBegan())
        {
            m_swallowedUntilReleased.emplace_back(channelName);
            Cancel();
            return true;
        }

        if (Contains(m_pressedAfterArming, channelName) && inputChannel.IsActive())
        {
            const AZStd::optional<ChannelInfo> info = InputChannelInfo::Find(channelName);
            if (info && info->m_deviceGroup == m_target->m_group && info->m_kind == m_target->m_kind &&
                IsPastThreshold(inputChannel, m_target->m_kind))
            {
                const Target target = *m_target;
                m_swallowedUntilReleased.emplace_back(channelName);
                StopWaiting();
                // Last: a handler may start the next capture.
                RebindableInputNotificationBus::Broadcast(&RebindableInputNotifications::OnChannelCaptured,
                    target.m_actionName, target.m_group, target.m_slot, AZStd::string(channelName));
                return true;
            }
        }

        // Everything else is swallowed while waiting.
        return true;
    }

    bool InputCapture::IsPastThreshold(const AzFramework::InputChannel& inputChannel, ChannelKind kind)
    {
        const float value = AZ::GetAbs(inputChannel.GetValue());
        if (kind == ChannelKind::MouseAxis)
        {
            // The wheel sends one event per notch: any notch counts.
            if (inputChannel.GetInputChannelId() == AzFramework::InputDeviceMouse::Movement::Z)
            {
                return value > 0.0f;
            }
            return value >= MouseMovementThreshold;
        }
        // Digital keys and buttons are 1 when pressed.
        return value >= AnalogThreshold;
    }

    void InputCapture::StopWaiting()
    {
        m_target.reset();
        m_armed = false;
        m_pressedAfterArming.clear();
        ++m_generation;
        DisconnectIfIdle();
    }

    void InputCapture::ConnectToInput()
    {
        if (!m_connected)
        {
            Connect();
            m_connected = true;
        }
    }

    void InputCapture::DisconnectIfIdle()
    {
        if (m_connected && !m_target && m_swallowedUntilReleased.empty())
        {
            Disconnect();
            m_connected = false;
        }
    }

} // namespace RebindableInput
