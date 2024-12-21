/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/Protocols/XdgManager.h>
#include <AzFramework/WaylandConnectionManager.h>

namespace AzFramework
{
    XdgManagerImpl::XdgManagerImpl()
    {
        WaylandRegistryEventsBus::Handler::BusConnect();
    }

    XdgManagerImpl::~XdgManagerImpl()
    {
        WaylandRegistryEventsBus::Handler::BusDisconnect();

        if (XdgShellConnectionManagerInterface::Get() == this)
        {
            XdgShellConnectionManagerInterface::Unregister(this);
        }

        if (XdgDecorConnectionManagerInterface::Get() == this)
        {
            XdgDecorConnectionManagerInterface::Unregister(this);
        }

        if (m_xdg != nullptr)
        {
            xdg_wm_base_destroy(m_xdg);
        }
        if (m_decor != nullptr)
        {
            zxdg_decoration_manager_v1_destroy(m_decor);
        }
    }

    uint32_t XdgManagerImpl::GetXdgWmBaseRegistryId() const
    {
        return m_xdgId;
    }

    xdg_wm_base* XdgManagerImpl::GetXdgWmBase() const
    {
        return m_xdg;
    }

    zxdg_decoration_manager_v1* XdgManagerImpl::GetXdgDecor() const
    {
        return m_decor;
    }
    void XdgManagerImpl::OnRegister(wl_registry* registry, uint32_t id, const char* interface, uint32_t version)
    {
        if (WL_IS_INTERFACE(xdg_wm_base_interface))
        {
            m_xdg = static_cast<xdg_wm_base*>(wl_registry_bind(registry, id, &xdg_wm_base_interface, version));
            xdg_wm_base_add_listener(m_xdg, &s_xdg_wm_listener, this);
            m_xdgId = id;
            WaylandInterfaceNotificationsBus::MultiHandler::BusConnect(m_xdgId);

            if (XdgShellConnectionManagerInterface::Get() == nullptr)
            {
                XdgShellConnectionManagerInterface::Register(this);
            }
        }
        else if (WL_IS_INTERFACE(zxdg_decoration_manager_v1_interface))
        {
            m_decor =
                static_cast<zxdg_decoration_manager_v1*>(wl_registry_bind(registry, id, &zxdg_decoration_manager_v1_interface, version));
            m_decorId = id;
            WaylandInterfaceNotificationsBus::MultiHandler::BusConnect(m_decorId);

            if (XdgDecorConnectionManagerInterface::Get() == nullptr)
            {
                XdgDecorConnectionManagerInterface::Register(this);
            }
        }
    }
    void XdgManagerImpl::OnUnregister(wl_registry*, uint32_t id)
    {
        if (m_xdgId == id)
        {
            WaylandInterfaceNotificationsBus::MultiHandler::BusDisconnect(m_xdgId);
            m_xdgId = 0;
            xdg_wm_base_destroy(m_xdg);
            m_xdg = nullptr;

            if (XdgShellConnectionManagerInterface::Get() == this)
            {
                XdgShellConnectionManagerInterface::Unregister(this);
            }
        }
        else if (m_decorId == id)
        {
            WaylandInterfaceNotificationsBus::MultiHandler::BusDisconnect(m_decorId);
            m_decorId = 0;
            zxdg_decoration_manager_v1_destroy(m_decor);
            m_decor = nullptr;

            if (XdgDecorConnectionManagerInterface::Get() == this)
            {
                XdgDecorConnectionManagerInterface::Unregister(this);
            }
        }
    }

    void XdgManagerImpl::OnProtocolError(uint32_t registryId, uint32_t errorCode)
    {
        auto conn = WaylandConnectionManagerInterface::Get();
        if (!conn)
        {
            return;
        }
        if (m_xdgId == registryId)
        {
            switch (errorCode)
            {
            case XDG_WM_BASE_ERROR_ROLE:
                AZ_Error("XDG", false, "Given surface has other role.");
                break;
            case XDG_WM_BASE_ERROR_DEFUNCT_SURFACES:
                AZ_Error("XDG", false, "xdg_wm_base was destroyed before children");
                break;
            case XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP:
                AZ_Error("XDG", false, "Tried to map or destroy a non-topmost popup");
                break;
            case XDG_WM_BASE_ERROR_INVALID_POPUP_PARENT:
                AZ_Error("XDG", false, "Invalid popup parent surface.");
                break;
            case XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE:
                AZ_Error("XDG", false, "Invalid surface state");
                break;
            case XDG_WM_BASE_ERROR_INVALID_POSITIONER:
                AZ_Error("XDG", false, "Invalid positioner.");
                break;
            case XDG_WM_BASE_ERROR_UNRESPONSIVE:
                AZ_Error("XDG", false, "Didn't ping in time");
                break;
            default:
                AZ_Error("XDG", false, "Unknown error");
                break;
            }
        }
        else if (m_decorId == registryId)
        {
            switch (errorCode)
            {
            case ZXDG_TOPLEVEL_DECORATION_V1_ERROR_UNCONFIGURED_BUFFER:
                AZ_Error("XDG Decor", false, "TopLevel has a buffer attached before configure.");
                break;
            case ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED:
                AZ_Error("XDG Decor", false, "TopLevel already has a decoration object.");
                break;
            case ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ORPHANED:
                AZ_Error("XDG Decor", false, "TopLevel destroyed before the decoration object.");
                break;
            default:
                AZ_Error("XDG Decor", false, "Unknown error");
                break;
            }
        }
    }

    void XdgManagerImpl::XdgPing(void* data, xdg_wm_base* xdg, uint32_t serial)
    {
        // You ping, I pong :)
        xdg_wm_base_pong(xdg, serial);
    }
} // namespace AzFramework