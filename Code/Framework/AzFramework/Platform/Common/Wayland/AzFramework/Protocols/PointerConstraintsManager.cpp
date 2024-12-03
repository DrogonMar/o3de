/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/Protocols/PointerConstraintsManager.h>

namespace AzFramework
{
    PointerConstraintsManagerImpl::PointerConstraintsManagerImpl()
    {
        WaylandRegistryEventsBus::Handler::BusConnect();

        PointerConstraintsManagerInterface::Register(this);
    }

    PointerConstraintsManagerImpl::~PointerConstraintsManagerImpl()
    {
        WaylandRegistryEventsBus::Handler::BusDisconnect();

        if (PointerConstraintsManagerInterface::Get() == this)
        {
            PointerConstraintsManagerInterface::Unregister(this);
        }
    }

    void PointerConstraintsManagerImpl::OnRegister(wl_registry* registry, uint32_t id, const char* interface, uint32_t version)
    {
        if (strcmp(interface, zwp_pointer_constraints_v1_interface.name) != 0)
        {
            return;
        }

        m_constraintsManager =
            static_cast<zwp_pointer_constraints_v1*>(wl_registry_bind(registry, id, &zwp_pointer_constraints_v1_interface, version));
        m_constraintsManagerId = id;
    }

    void PointerConstraintsManagerImpl::OnUnregister(wl_registry* registry, uint32_t id)
    {
        if (m_constraintsManagerId == id)
        {
            zwp_pointer_constraints_v1_destroy(m_constraintsManager);
            m_constraintsManager = nullptr;
            m_constraintsManagerId = 0;
        }
    }

    zwp_pointer_constraints_v1* PointerConstraintsManagerImpl::GetConstraints()
    {
        return m_constraintsManager;
    }
} // namespace AzFramework