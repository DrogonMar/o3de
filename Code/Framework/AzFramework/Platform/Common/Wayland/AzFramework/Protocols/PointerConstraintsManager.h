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
#include <AzFramework/Protocols/Gen/pointer-constraints-client-protocol.h>

namespace AzFramework
{
	class PointerConstraintsManager
	{
	public:
		AZ_RTTI(PointerConstraintsManager, "{C22DB3C9-6059-42D1-8D82-6FA2018FA078}");

		virtual ~PointerConstraintsManager() = default;

		virtual zwp_pointer_constraints_v1* GetConstraints() = 0;
	};

	using PointerConstraintsManagerInterface = AZ::Interface<PointerConstraintsManager>;
}