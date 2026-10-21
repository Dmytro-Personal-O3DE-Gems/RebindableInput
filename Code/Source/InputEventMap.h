/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * Forked from Gems/StartingPointInput (O3DE, tag 2605.0) into the RebindableInput gem.
 * Changes: moved to the RebindableInput namespace, new type ids, Script Canvas parts removed.
 *
 */
#pragma once
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Component/TickBus.h>
#include <AzFramework/Input/Buses/Notifications/InputDeviceNotificationBus.h>
#include <AzFramework/Input/Events/InputChannelEventListener.h>

#include <StartingPointInput/InputEventNotificationBus.h>

namespace AZ
{
    class ReflectContext;
}

namespace RebindableInput
{
    // The input events are still sent on StartingPointInput's bus, so every existing listener
    // (InputEventNotificationBus::MultiHandler with InputEventNotificationId{"..."}) keeps working unchanged.
    using StartingPointInput::InputEventNotificationBus;
    using StartingPointInput::InputEventNotificationId;
    using StartingPointInput::InputEventNotifications;

    //////////////////////////////////////////////////////////////////////////
    /// Classes that inherit from this one will share the life-cycle API's
    /// with components.  Components that contain the subclasses are expected
    /// to call these methods in their Init/Activate/Deactivate methods
    //////////////////////////////////////////////////////////////////////////
    class InputSubComponent
    {
    public:
        AZ_RTTI(InputSubComponent, "{7C03BBB9-15DF-46AB-879A-B35960D0CCC2}");
        virtual ~InputSubComponent() = default;

        //////////////////////////////////////////////////////////////////////////
        /// InputSubComponents will share the life-cycle API's of components.
        /// Any Component that contains an InputSubComponent is expected to call
        /// these methods in their Activate/Deactivate methods
        virtual void Activate(const InputEventNotificationId& channel) = 0;
        virtual void Deactivate(const InputEventNotificationId& channel) = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    /// Maps raw input from any raw input source and outputs Pressed, Held, and Released input events
    class InputEventMap
        : public InputSubComponent
        , protected AzFramework::InputChannelEventListener
    {
    public:
        InputEventMap();
        ~InputEventMap() override = default;
        AZ_RTTI(InputEventMap, "{7C4D4A6D-2833-4BEC-8BD3-D003349663A8}", InputSubComponent);
        static void Reflect(AZ::ReflectContext* reflection);

        //////////////////////////////////////////////////////////////////////////
        // InputSubComponent
        void Activate(const InputEventNotificationId& eventNotificationId) override;
        void Deactivate(const InputEventNotificationId& eventNotificationId) override;

    protected:
        AZStd::string GetEditorText() const;
        virtual const AZStd::vector<AZStd::string> GetInputDeviceTypes() const;
        virtual const AZStd::vector<AZStd::string> GetInputNamesBySelectedDevice() const;

        AZ::Crc32 OnDeviceSelected();

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::InputChannelEventListener
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;

        using InputEventType = void(InputEventNotificationBus::Events::*)(float);
        virtual float CalculateEventValue(const AzFramework::InputChannel& inputChannel) const;
        void SendEventsInternal(float value, const AzFramework::LocalUserId& localUserIdOfEvent, const InputEventNotificationId busId, InputEventType eventType);

        //////////////////////////////////////////////////////////////////////////
        // Non Reflected Data
        InputEventNotificationId m_outgoingBusId;
        bool m_wasPressed = false;

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        float m_eventValueMultiplier = 1.f;
        AZStd::string m_inputName = "";
        AZStd::string m_inputDeviceType = "";
        float m_deadZone = 0.0f;
    };

    //////////////////////////////////////////////////////////////////////////
    /// ThumbstickInput handles raw input from thumbstick sources, applies any
    /// custom dead-zone or sensitivity curve calculations, and then outputs
    /// Pressed, Held, and Released input events for the specified axis
    class ThumbstickInputEventMap : public InputEventMap
    {
    public:
        ThumbstickInputEventMap();
        ~ThumbstickInputEventMap() override = default;
        AZ_RTTI(ThumbstickInputEventMap, "{30B0CA40-3ABD-4630-BE22-C5B48205C5CA}", InputEventMap);
        static void Reflect(AZ::ReflectContext* reflection);

    protected:
        AZStd::string GetEditorText() const;
        const AZStd::vector<AZStd::string> GetInputDeviceTypes() const override;
        const AZStd::vector<AZStd::string> GetInputNamesBySelectedDevice() const override;
        bool OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel) override;
        float CalculateEventValue(const AzFramework::InputChannel& inputChannel) const override;

        static AZ::Vector2 ApplyDeadZonesAndSensitivity(const AZ::Vector2& inputValues,
                                                        float innerDeadZone,
                                                        float outerDeadZone,
                                                        float axisDeadZone,
                                                        float sensitivityExponent);

        enum class OutputAxis
        {
            X,
            Y
        };

        //////////////////////////////////////////////////////////////////////////
        // Non Reflected Data
        const AzFramework::InputDeviceId* m_wasLastPressedByInputDeviceId = nullptr;

        //////////////////////////////////////////////////////////////////////////
        // Reflected Data
        float m_innerDeadZoneRadius = 0.0f;
        float m_outerDeadZoneRadius = 1.0f;
        float m_axisDeadZoneValue = 0.0f;
        float m_sensitivityExponent = 1.0f;
        OutputAxis m_outputAxis = OutputAxis::X;
    };
} // namespace RebindableInput
