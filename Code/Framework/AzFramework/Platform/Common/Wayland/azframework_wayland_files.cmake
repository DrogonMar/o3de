#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(FILES
    AzFramework/WaylandApplication.cpp
    AzFramework/WaylandApplication.h
    AzFramework/WaylandConnectionManager.h
    AzFramework/WaylandNativeWindow.cpp
    AzFramework/WaylandNativeWindow.h

    AzFramework/WaylandInputDeviceMouse.cpp
    AzFramework/WaylandInputDeviceMouse.h
    AzFramework/WaylandInputDeviceKeyboard.cpp
    AzFramework/WaylandInputDeviceKeyboard.h

    AzFramework/Protocols/OutputManager.h
    AzFramework/Protocols/SeatManager.h

    AzFramework/Protocols/XdgShellManager.h
    AzFramework/Protocols/Gen/xdg-shell-client-protocol.c
    AzFramework/Protocols/Gen/xdg-shell-client-protocol.h

    AzFramework/Protocols/XdgDecorManager.h
    AzFramework/Protocols/Gen/xdg-decor-client-protocol.c
    AzFramework/Protocols/Gen/xdg-decor-client-protocol.h

    AzFramework/Protocols/CursorShapeManager.h
    AzFramework/Protocols/Gen/cursor-shape-client-protocol.c
    AzFramework/Protocols/Gen/cursor-shape-client-protocol.h
    #Really just to make cursor shape happy.
    AzFramework/Protocols/Gen/tablet-client-protocol.c
    AzFramework/Protocols/Gen/tablet-client-protocol.h

    AzFramework/Protocols/PointerConstraintsManager.h
    AzFramework/Protocols/Gen/pointer-constraints-client-protocol.c
    AzFramework/Protocols/Gen/pointer-constraints-client-protocol.h

    AzFramework/Protocols/RelativePointerManager.h
    AzFramework/Protocols/Gen/relative-pointer-client-protocol.c
    AzFramework/Protocols/Gen/relative-pointer-client-protocol.h
)
