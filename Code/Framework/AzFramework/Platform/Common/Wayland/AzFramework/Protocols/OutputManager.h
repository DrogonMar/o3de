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
#include <AzFramework/Protocols/Gen/xdg-shell-client-protocol.h>

namespace AzFramework
{
	class OutputManager
	{
	public:
		AZ_RTTI(OutputManager, "{8EEBB5C3-91BA-4AE8-8529-6F0FE69E7E9B}");

		virtual ~OutputManager() = default;

		virtual uint32_t GetRefreshRateMhz(wl_output* output) = 0;
		virtual AZStd::string GetOutputName(wl_output* output) = 0;
		virtual AZStd::string GetOutputDesc(wl_output* output) = 0;
	};

	using OutputManagerInterface = AZ::Interface<OutputManager>;
}