/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/API/ApplicationAPI_Platform.h>
#include <AzFramework/Application/Application.h>
#include <AzFramework/Protocols/OutputManager.h>
#include <AzFramework/WaylandConnectionManager.h>

// Notes: XdgShell is optional
namespace AzFramework
{
    class WaylandApplication final
        : public Application::Implementation
        , public LinuxLifecycleEvents::Bus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(WaylandApplication, AZ::SystemAllocator);
        WaylandApplication();
        ~WaylandApplication() override;

        bool HasEventsWaiting() const;

        void PumpSystemEventLoopOnce() override;
        void PumpSystemEventLoopUntilEmpty() override;

    private:
        AZStd::unique_ptr<WaylandConnectionManager> m_waylandConnectionManager;
        AZStd::unique_ptr<OutputManager> m_outputManager;

        // These are protocol managers we will keep alive
        AZStd::unique_ptr<class XdgManagerImpl> m_xdgManager;
        AZStd::unique_ptr<class RelativePointerManager> m_relativePointerManager;
        AZStd::unique_ptr<class PointerConstraintsManager> m_pointerConstraintsManager;
        AZStd::unique_ptr<class CursorShapeManager> m_cursorShapeManager;
    };
} // namespace AzFramework