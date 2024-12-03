/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "WaylandInputDeviceMouse.h"

#include <AzFramework/Protocols/SeatManager.h>

#include <linux/input-event-codes.h>

namespace AzFramework
{
    // Using the accelerated values should be default for relative pointer
    AZ_CVAR(
        bool,
        wl_accel,
        1,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "WAYLAND ONLY: Set to use accelerated values, this only works if the compositor supports relative pointer.");

    void WaylandInputDeviceMouse::PointerEnter(
        void* data,
        struct wl_pointer* /*wl_pointer*/,
        uint32_t serial,
        struct wl_surface* surface,
        wl_fixed_t surface_x,
        wl_fixed_t surface_y)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);
        if (surface != nullptr)
        {
            if (auto wnw = static_cast<WaylandNativeWindow*>(wl_surface_get_user_data(surface)))
            {
                wnw->SetPointerFocus(self);
                self->m_focusedWindow = wnw;
            }
        }

        self->m_currentSerial = serial;

        self->m_axisEvent.m_eventMask |= PointerEventMask::POINTER_EVENT_ENTER;
        self->m_axisEvent.m_serial = serial;
        self->m_axisEvent.m_surfaceX = surface_x;
        self->m_axisEvent.m_surfaceY = surface_y;

        self->InternalApplyCursorState();
    }

    void WaylandInputDeviceMouse::PointerLeave(void* data, struct wl_pointer* /*wl_pointer*/, uint32_t serial, struct wl_surface* surface)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);

        if (surface != nullptr)
        {
            if (auto wnw = static_cast<WaylandNativeWindow*>(wl_surface_get_user_data(surface)))
            {
                wnw->SetPointerFocus(nullptr);
            }
        }
        self->m_focusedWindow = nullptr;

        self->m_currentSerial = UINT32_MAX;

        self->m_axisEvent.m_eventMask |= PointerEventMask::POINTER_EVENT_LEAVE;
        self->m_axisEvent.m_serial = serial;
    }

    void WaylandInputDeviceMouse::PointerMotion(
        void* data, struct wl_pointer* /*wl_pointer*/, uint32_t /*time*/, wl_fixed_t surface_x, wl_fixed_t surface_y)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);
        self->m_position = AZ::Vector2((float)wl_fixed_to_double(surface_x), (float)wl_fixed_to_double(surface_y));
        if (self->m_focusedWindow)
        {
            self->m_position *= self->m_focusedWindow->GetDpiScaleFactor();
        }
    }

    void WaylandInputDeviceMouse::PointerButton(
        void* data, struct wl_pointer* /*wl_pointer*/, uint32_t /*serial*/, uint32_t /*time*/, uint32_t button, uint32_t state)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);
        bool pressed = state & wl_pointer_button_state::WL_POINTER_BUTTON_STATE_PRESSED;

        switch (button)
        {
        case BTN_LEFT:
            self->QueueRawButtonEvent(InputDeviceMouse::Button::Left, pressed);
            break;
        case BTN_RIGHT:
            self->QueueRawButtonEvent(InputDeviceMouse::Button::Right, pressed);
            break;
        case BTN_MIDDLE:
            self->QueueRawButtonEvent(InputDeviceMouse::Button::Middle, pressed);
            break;
        case BTN_SIDE:
            self->QueueRawButtonEvent(InputDeviceMouse::Button::Other1, pressed);
            break;
        case BTN_EXTRA:
            self->QueueRawButtonEvent(InputDeviceMouse::Button::Other2, pressed);
            break;
        default:
            break;
        }
    }

    void WaylandInputDeviceMouse::PointerAxis(void* data, struct wl_pointer* /*wl_pointer*/, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);
        self->m_axisEvent.m_eventMask |= POINTER_EVENT_AXIS;
        self->m_axisEvent.m_time = time;
        self->m_axisEvent.m_axis[axis].m_valid = true;
        self->m_axisEvent.m_axis[axis].m_value = value;
    }

    void WaylandInputDeviceMouse::PointerAxisSource(void* data, struct wl_pointer* /*wl_pointer*/, uint32_t axis_source)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);
        self->m_axisEvent.m_eventMask |= POINTER_EVENT_AXIS_SOURCE;
        self->m_axisEvent.m_axisSource = axis_source;
    }

    void WaylandInputDeviceMouse::PointerAxisStop(void* data, struct wl_pointer* /*wl_pointer*/, uint32_t time, uint32_t axis)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);
        self->m_axisEvent.m_time = time;
        self->m_axisEvent.m_eventMask |= POINTER_EVENT_AXIS_STOP;
        self->m_axisEvent.m_axis[axis].m_valid = true;
    }

    void WaylandInputDeviceMouse::PointerAxisDiscrete(void* data, struct wl_pointer* /*wl_pointer*/, uint32_t axis, int32_t discrete)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);
        self->m_axisEvent.m_eventMask |= POINTER_EVENT_AXIS_DISCRETE;
        self->m_axisEvent.m_axis[axis].m_valid = true;
        self->m_axisEvent.m_axis[axis].m_discrete = discrete;
    }

    void WaylandInputDeviceMouse::PointerAxisValue120(
        void* /*data*/, struct wl_pointer* /*wl_pointer*/, uint32_t /*axis*/, int32_t /*value120*/)
    {
    }

    void WaylandInputDeviceMouse::PointerAxisRelDir(
        void* /*data*/, struct wl_pointer* /*wl_pointer*/, uint32_t /*axis*/, uint32_t /*direction*/)
    {
    }

    void WaylandInputDeviceMouse::PointerFrame(void* data, struct wl_pointer*)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);

        if (self->m_axisEvent.m_eventMask & POINTER_EVENT_AXIS)
        {
            const auto& verticalScrollEvent = self->m_axisEvent.m_axis[WL_POINTER_AXIS_VERTICAL_SCROLL];
            if (verticalScrollEvent.m_valid)
            {
                self->QueueRawMovementEvent(InputDeviceMouse::Movement::Z, -(float)wl_fixed_to_double(verticalScrollEvent.m_value) * 8.0f);
            }
        }

        self->m_axisEvent = {};
    }

    void WaylandInputDeviceMouse::RelPointerMotion(
        void* data,
        struct zwp_relative_pointer_v1*,
        uint32_t,
        uint32_t,
        wl_fixed_t dx,
        wl_fixed_t dy,
        wl_fixed_t dx_unaccel,
        wl_fixed_t dy_unaccel)
    {
        auto self = static_cast<WaylandInputDeviceMouse*>(data);

        wl_fixed_t x = dx_unaccel;
        wl_fixed_t y = dy_unaccel;

        if (wl_accel)
        {
            x = dx;
            y = dy;
        }

        if (x != 0)
        {
            self->QueueRawMovementEvent(InputDeviceMouse::Movement::X, (float)wl_fixed_to_double(x));
        }
        if (y != 0)
        {
            self->QueueRawMovementEvent(InputDeviceMouse::Movement::Y, (float)wl_fixed_to_double(y));
        }
    }

    WaylandInputDeviceMouse::WaylandInputDeviceMouse(AzFramework::InputDeviceMouse& inputDevice)
        : InputDeviceMouse::Implementation(inputDevice)
        , m_playerIdx(inputDevice.GetInputDeviceId().GetIndex())
        , m_cursorState(SystemCursorState::UnconstrainedAndVisible)
    {
        SeatCapsChanged();
        SeatNotificationsBus::Handler::BusConnect(m_playerIdx);

        if (auto wl = WaylandConnectionManagerInterface::Get())
        {
            m_confinedRegion = wl_compositor_create_region(wl->GetWaylandCompositor());
        }
    }

    WaylandInputDeviceMouse::~WaylandInputDeviceMouse()
    {
        SeatNotificationsBus::Handler::BusDisconnect();
        UpdatePointer(nullptr);
    }

    WaylandInputDeviceMouse::Implementation* WaylandInputDeviceMouse::Create(AzFramework::InputDeviceMouse& inputDevice)
    {
        return aznew WaylandInputDeviceMouse(inputDevice);
    }

    void WaylandInputDeviceMouse::UpdatePointer(wl_pointer* newPointer)
    {
        if (newPointer == m_pointer)
        {
            return;
        }

        if (m_pointer != nullptr)
        {
            if (m_shapeDevice != nullptr)
            {
                wp_cursor_shape_device_v1_destroy(m_shapeDevice);
                m_shapeDevice = nullptr;
            }
            if (m_relPointer != nullptr)
            {
                zwp_relative_pointer_v1_destroy(m_relPointer);
                m_relPointer = nullptr;
            }
            wl_pointer_release(m_pointer);
        }

        m_pointer = newPointer;
        if (m_pointer)
        {
            wl_pointer_add_listener(m_pointer, &s_pointer_listener, this);

            if (auto cursorManager = CursorShapeManagerInterface::Get())
            {
                m_shapeDevice = cursorManager->GetCursorShapeDevice(m_pointer);
            }
            if (auto relManager = RelativePointerManagerInterface::Get())
            {
                m_relPointer = relManager->GetRelativePointer(m_pointer);
                if (m_relPointer != nullptr)
                {
                    zwp_relative_pointer_v1_set_user_data(m_relPointer, this);
                    zwp_relative_pointer_v1_add_listener(m_relPointer, &s_rel_pointer_listener, this);
                }
            }
        }
    }

    void WaylandInputDeviceMouse::ReleaseSeat()
    {
        UpdatePointer(nullptr);
    }

    void WaylandInputDeviceMouse::SeatCapsChanged()
    {
        const auto* interface = AzFramework::SeatManagerInterface::Get();
        if (!interface)
        {
            return;
        }

        UpdatePointer(interface->GetSeatPointer(m_playerIdx));
    }

    bool WaylandInputDeviceMouse::IsConnected() const
    {
        return m_pointer != nullptr;
    }

    void WaylandInputDeviceMouse::SetSystemCursorState(AzFramework::SystemCursorState systemCursorState)
    {
        m_cursorState = systemCursorState;
        InternalApplyCursorState();
    }

    SystemCursorState WaylandInputDeviceMouse::GetSystemCursorState() const
    {
        return m_cursorState;
    }

    void WaylandInputDeviceMouse::SetSystemCursorPositionNormalized(AZ::Vector2)
    {
        // We can do this when locked but only as a hint to the compositor
        // that we want the cursor to show up at that location when unlocked.
        // I think O3DE uses this to warp the cursor somewhere when unlocked which in that case
        // won't work.
    }

    AZ::Vector2 WaylandInputDeviceMouse::GetSystemCursorPositionNormalized() const
    {
        if (m_focusedWindow)
        {
            const auto windowSize = m_focusedWindow->GetClientAreaSize();
            return m_position / AZ::Vector2((float)windowSize.m_width, (float)windowSize.m_height);
        }

        return m_position;
    }

    void WaylandInputDeviceMouse::TickInputDevice()
    {
        ProcessRawEventQueues();
    }

    void WaylandInputDeviceMouse::InternalApplyCursorState()
    {
        if (m_currentSerial == UINT32_MAX)
        {
            // Need a serial code
            return;
        }
        switch (m_cursorState)
        {
        case SystemCursorState::ConstrainedAndHidden:
            InternalSetShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, false);
            InternalConstrainMouse(true);
            break;

        case SystemCursorState::ConstrainedAndVisible:
            InternalSetShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, true);
            InternalConstrainMouse(true);
            break;

        case SystemCursorState::UnconstrainedAndVisible:
            InternalSetShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, true);
            InternalConstrainMouse(false);
            break;

        case SystemCursorState::UnconstrainedAndHidden:
            InternalSetShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, false);
            InternalConstrainMouse(false);
            break;
        default:
            InternalSetShape(WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT, true);
            InternalConstrainMouse(false);
            break;
        }
    }

    void WaylandInputDeviceMouse::InternalSetShape(const wp_cursor_shape_device_v1_shape shape, bool visible)
    {
        if (!visible)
        {
            wl_pointer_set_cursor(m_pointer, m_currentSerial, nullptr, 0, 0);
            return;
        }

        if (m_shapeDevice == nullptr)
        {
            return;
        }

        if (m_currentSerial == UINT32_MAX)
        {
            return;
        }

        wp_cursor_shape_device_v1_set_shape(m_shapeDevice, m_currentSerial, shape);
    }

    void WaylandInputDeviceMouse::InternalConstrainMouse(bool wantConstraints)
    {
        if (wantConstraints)
        {
            if (m_lockedPointer != nullptr)
            {
                // Already confined
                return;
            }

            if (m_focusedWindow == nullptr)
            {
                // No focused window!
                return;
            }

            if (auto constraintsManager = PointerConstraintsManagerInterface::Get())
            {
                void* constraintWindowRawPtr = nullptr;
                InputSystemCursorConstraintRequestBus::BroadcastResult(
                    constraintWindowRawPtr, &InputSystemCursorConstraintRequestBus::Events::GetSystemCursorConstraintWindow);
                auto constraintWlWindow = (WaylandNativeWindow*)constraintWindowRawPtr;
                if (constraintWlWindow == nullptr)
                {
                    // Use focused
                    constraintWlWindow = m_focusedWindow;
                }

                // Remove our old region
                wl_region_subtract(
                    m_confinedRegion,
                    (int32_t)m_currentRegion.m_posX,
                    (int32_t)m_currentRegion.m_posY,
                    (int32_t)m_currentRegion.m_width,
                    (int32_t)m_currentRegion.m_height);

                const auto constraintSize = constraintWlWindow->GetClientAreaSize();
                m_currentRegion = { 0, 0, constraintSize.m_width, constraintSize.m_height };
                wl_region_add(
                    m_confinedRegion,
                    (int32_t)m_currentRegion.m_posX,
                    (int32_t)m_currentRegion.m_posY,
                    (int32_t)m_currentRegion.m_width,
                    (int32_t)m_currentRegion.m_height);

                m_lockedPointer = zwp_pointer_constraints_v1_lock_pointer(
                    constraintsManager->GetConstraints(),
                    (wl_surface*)m_focusedWindow->GetWindowHandle(),
                    m_pointer,
                    m_confinedRegion,
                    ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);

                return;
            }
        }
        else
        {
            if (m_lockedPointer)
            {
                // Dont want constraints anymore
                zwp_locked_pointer_v1_destroy(m_lockedPointer);
                m_lockedPointer = nullptr;
                return;
            }
        }
    }

} // namespace AzFramework