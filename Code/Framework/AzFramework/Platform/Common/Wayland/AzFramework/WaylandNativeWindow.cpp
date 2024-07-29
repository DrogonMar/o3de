/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/Application/Application.h>
#include <AzFramework/Windowing/NativeWindow.h>
#include <AzFramework/WaylandConnectionManager.h>
#include <AzFramework/WaylandInterface.h>
#include <AzFramework/WaylandNativeWindow.h>
#include <AzFramework/Protocols/OutputManager.h>

#include <wayland-client.hpp>

namespace AzFramework
{
	[[maybe_unused]] const char WaylandErrorWindow[] = "WaylandNativeWindow";


	void WaylandNativeWindow::SurfaceEnter(void *data,
							 struct wl_surface */*wl_surface*/,
							 struct wl_output *output)
	{
		auto self = (AzFramework::WaylandNativeWindow*)data;

		if(auto outputManager = AzFramework::OutputManagerInterface::Get()){
			uint32_t refreshRateMhz = outputManager->GetRefreshRateMhz(output);

			if(refreshRateMhz == 0){
				return;
			}

			self->m_currentEnteredOutput = output;
			self->UpdateRefreshRate(refreshRateMhz);
			AZStd::string name = outputManager->GetOutputName(output); // DP-1 or HDMI
			AZStd::string desc = outputManager->GetOutputDesc(output); // Name of monitor.
			AZ_Info(WaylandErrorWindow, "Entered screen: %s - %s", desc.c_str(), name.c_str());
		}
	}

	void WaylandNativeWindow::SurfaceLeave(void */*data*/,
							 struct wl_surface */*wl_surface*/,
							 struct wl_output */*output*/)
	{

	}

	void WaylandNativeWindow::SurfacePreferredScale(void */*data*/,
									  struct wl_surface */*wl_surface*/,
									  int32_t /*factor*/)
	{

	}

	void WaylandNativeWindow::SurfacePreferredTransform(void */*data*/,
										  struct wl_surface */*wl_surface*/,
										  uint32_t /*transform*/)
	{

	}

	void WaylandNativeWindow::XdgSurfaceConfigure(void* /*data*/, struct xdg_surface* xdg_surface, uint32_t serial){
		xdg_surface_ack_configure(xdg_surface, serial);
	}

	void WaylandNativeWindow::XdgTopLevelConfigure(void *,
									 struct xdg_toplevel *,
									 int32_t ,
									 int32_t ,
									 struct wl_array *)
	{

	}

	void WaylandNativeWindow::XdgTopLevelClose(void *data, struct xdg_toplevel */*xdg_toplevel*/){
		auto self = (AzFramework::WaylandNativeWindow*)data;
		self->Deactivate();
	}

	void WaylandNativeWindow::XdgTopLevelConfigureBounds(void *,
										   struct xdg_toplevel *,
										   int32_t ,
										   int32_t )
	{

	}

	void WaylandNativeWindow::XdgTopLevelWmCaps(void *,
								  struct xdg_toplevel *,
								  struct wl_array *)
	{

	}

	WaylandNativeWindow::WaylandNativeWindow()
		: NativeWindow::Implementation()
		, m_display(nullptr)
		, m_compositor(nullptr)
		, m_surface(nullptr)
		, m_xdgSurface(nullptr)
	{
		if(auto connectionManager = AzFramework::WaylandConnectionManagerInterface::Get(); connectionManager != nullptr)
		{
			m_display = connectionManager->GetWaylandDisplay();
			m_compositor = connectionManager->GetWaylandCompositor();
		}
		if(auto xdgShellManager = AzFramework::XdgShellConnectionManagerInterface::Get(); xdgShellManager != nullptr){
			m_xdgShell = xdgShellManager->GetXdgWmBase();
		}
		if(auto xdgDecorManager = AzFramework::XdgDecorConnectionManagerInterface::Get(); xdgDecorManager != nullptr){
			m_xdgDecor = xdgDecorManager->GetXdgDecor();
		}
		AZ_Error(WaylandErrorWindow, m_display != nullptr, "Unable to get Wayland display.");
		AZ_Error(WaylandErrorWindow, m_compositor != nullptr, "Unable to get Wayland compositor.");
	}

	WaylandNativeWindow::~WaylandNativeWindow()
	{
	}

	void WaylandNativeWindow::Activate()
	{
		if(!m_activated && m_surface != nullptr)
		{
			wl_surface_commit(m_surface);
			m_activated = true;
		}
	}

	void WaylandNativeWindow::Deactivate()
	{
		if(!m_activated)
		{
			return;
		}

		WindowNotificationBus::Event(reinterpret_cast<NativeWindowHandle>(m_surface), &WindowNotificationBus::Events::OnWindowClosed);

		if(m_xdgTopLevelDecor != nullptr){
			zxdg_toplevel_decoration_v1_destroy(m_xdgTopLevelDecor);
			m_xdgTopLevelDecor = nullptr;
		}
		if(m_xdgToplevel != nullptr){
			xdg_toplevel_destroy(m_xdgToplevel);
			m_xdgToplevel = nullptr;
		}
		if(m_xdgSurface != nullptr){
			xdg_surface_destroy(m_xdgSurface);
			m_xdgSurface = nullptr;
		}
		if(m_surface != nullptr){
			wl_surface_destroy(m_surface);
			m_surface = nullptr;
		}

		m_activated = false;
	}

	void WaylandNativeWindow::InitWindowInternal(const AZStd::string &title,
											 const WindowGeometry &geometry,
											 const WindowStyleMasks &styleMasks)
	{
		m_surface = wl_compositor_create_surface(m_compositor);
		AZ_Error(WaylandErrorWindow, m_surface != nullptr, "Failed to create surface.");
		wl_surface_set_user_data(m_surface, this);

		wl_surface_add_listener(m_surface, &s_surfaceListener, this);

		//It's possible we dont have access to XdgShell
		//it's a stable wayland protocol so any normal desktop compositor will have it
		if(m_xdgShell != nullptr) {
			m_xdgSurface = xdg_wm_base_get_xdg_surface(m_xdgShell, m_surface);
			AZ_Error(WaylandErrorWindow, m_xdgSurface != nullptr, "Failed to create XDG surface.");

			m_xdgToplevel = xdg_surface_get_toplevel(m_xdgSurface);
			AZ_Error(WaylandErrorWindow, m_xdgSurface != nullptr, "Failed to create XDG Toplevel surface.");

			xdg_surface_add_listener(m_xdgSurface, &s_xdgSurfaceListener, this);
			xdg_toplevel_add_listener(m_xdgToplevel, &s_xdgTopLevelListener, this);

			xdg_surface_set_window_geometry(m_xdgSurface, 0, 0, (int32_t)geometry.m_width, (int32_t)geometry.m_height);
			xdg_toplevel_set_title(m_xdgToplevel, title.c_str());

			const uint32_t mask = styleMasks.m_platformAgnosticStyleMask;
			if(mask & WindowStyleMasks::WINDOW_STYLE_RESIZEABLE){
				xdg_toplevel_set_min_size(m_xdgToplevel, 0, 0);
				xdg_toplevel_set_max_size(m_xdgToplevel, INT32_MAX, INT32_MAX);
			}else{
				xdg_toplevel_set_min_size(m_xdgToplevel, (int32_t)geometry.m_width, (int32_t)geometry.m_height);
				xdg_toplevel_set_max_size(m_xdgToplevel, (int32_t)geometry.m_width, (int32_t)geometry.m_height);
			}

			if(m_xdgDecor != nullptr){
				//We have decor support yippie
				m_xdgTopLevelDecor = zxdg_decoration_manager_v1_get_toplevel_decoration(m_xdgDecor, m_xdgToplevel);
				AZ_Error(WaylandErrorWindow, m_xdgTopLevelDecor != nullptr, "Failed to create XDG Toplevel decor.");

				if((mask & WindowStyleMasks::WINDOW_STYLE_BORDERED) || (mask & WindowStyleMasks::WINDOW_STYLE_RESIZEABLE)){
					zxdg_toplevel_decoration_v1_set_mode(m_xdgTopLevelDecor, zxdg_toplevel_decoration_v1_mode::ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
				}else{
					zxdg_toplevel_decoration_v1_set_mode(m_xdgTopLevelDecor, zxdg_toplevel_decoration_v1_mode::ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
				}
			}
		}

		m_width  = geometry.m_width;
		m_height = geometry.m_height;

		wl_surface_commit(m_surface);
	}

	NativeWindowHandle WaylandNativeWindow::GetWindowHandle() const
	{
		return reinterpret_cast<NativeWindowHandle>(m_surface);
	}


	void WaylandNativeWindow::SetWindowTitle(const AZStd::string &title)
	{
		if(m_xdgToplevel == nullptr)
			return;

		xdg_toplevel_set_title(m_xdgToplevel, title.c_str());
	}

	bool WaylandNativeWindow::SupportsClientAreaResize() const
	{
		return true;
	}

	float WaylandNativeWindow::GetDpiScaleFactor() const
	{
		return m_dpiScaleFactor;
	}

	uint32_t WaylandNativeWindow::GetDisplayRefreshRate() const
	{
		return m_currentRefreshFramerate;
	}

	void WaylandNativeWindow::UpdateRefreshRate(uint32_t newRefreshMhz)
	{
		m_currentRefreshMhz = newRefreshMhz;
		m_currentRefreshFramerate = (uint32_t)AZStd::ceil((float)m_currentRefreshMhz / 1000.0f);
		WindowNotificationBus::Event(
			GetWindowHandle(),
			&WindowNotificationBus::Events::OnRefreshRateChanged,
			m_currentRefreshFramerate);
	}

	void WaylandNativeWindow::UpdateScaleFactor(float newScale)
	{
		m_dpiScaleFactor = newScale;
		WindowNotificationBus::Event(
			GetWindowHandle(),
			&WindowNotificationBus::Events::OnDpiScaleFactorChanged,
			m_dpiScaleFactor);
	}

	bool WaylandNativeWindow::GetFullScreenState() const
	{
		return m_currentFullscreen != nullptr;
	}

	void WaylandNativeWindow::SetFullScreenState(bool fullScreenState)
	{
		if(m_xdgToplevel == nullptr)
		{
			//Cant do fullscreen.
			return;
		}

		if(fullScreenState)
		{
			//Just use what ever output we just entered
			xdg_toplevel_set_fullscreen(m_xdgToplevel, m_currentEnteredOutput);;
			m_currentFullscreen = m_currentEnteredOutput;
		}
		else{
			xdg_toplevel_unset_fullscreen(m_xdgToplevel);
			m_currentFullscreen = nullptr;
		}
	}

	void WaylandNativeWindow::SetPointerFocus(WaylandInputDeviceMouse *pointer)
	{
		m_focusedCursor = pointer;
	}

	void WaylandNativeWindow::SetKeyboardFocus(WaylandInputDeviceKeyboard *keyboard)
	{
		m_focusedKeyboard = keyboard;
	}

}
