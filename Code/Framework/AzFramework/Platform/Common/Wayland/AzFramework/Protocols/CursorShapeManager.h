/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>

#include <AzFramework/WaylandInterface.h>
#include <wayland-client.hpp>
#include <AzFramework/Protocols/Gen/cursor-shape-client-protocol.h>

namespace AzFramework
{
	class CursorShapeManager
	{
	public:
		AZ_RTTI(CursorShapeManager, "{569EF165-AB9D-4F81-8E79-CE0E69600B8F}");

		virtual ~CursorShapeManager() = default;

		virtual wp_cursor_shape_device_v1* GetCursorShapeDevice(wl_pointer* pointer) = 0;
	};

	using CursorShapeManagerInterface = AZ::Interface<CursorShapeManager>;
}