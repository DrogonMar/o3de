/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Memory/SystemAllocator.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>

#include <AzFramework/Protocols/Gen/relative-pointer-client-protocol.h>
#include <AzFramework/WaylandInterface.h>

namespace AzFramework
{
    class RelativePointerManager
    {
    public:
        AZ_RTTI(RelativePointerManager, "{AA4CD0C4-0140-406C-B2A7-A39AEFD88346}");

        virtual ~RelativePointerManager() = default;

        virtual zwp_relative_pointer_v1* GetRelativePointer(wl_pointer* pointer) = 0;
    };

    using RelativePointerManagerInterface = AZ::Interface<RelativePointerManager>;

    class RelativePointerManagerImpl
        : public WaylandRegistryEventsBus::Handler
        , public RelativePointerManager
    {
    public:
        AZ_CLASS_ALLOCATOR(RelativePointerManagerImpl, AZ::SystemAllocator);
        RelativePointerManagerImpl();
        ~RelativePointerManagerImpl() override;

        void OnRegister(wl_registry* registry, uint32_t id, const char* interface, uint32_t version) override;
        void OnUnregister(wl_registry* registry, uint32_t id) override;

        zwp_relative_pointer_v1* GetRelativePointer(wl_pointer* pointer) override;

    private:
        zwp_relative_pointer_manager_v1* m_relativePointerManager = nullptr;
        uint32_t m_relativePointerManagerId = 0;
    };
} // namespace AzFramework