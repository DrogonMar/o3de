/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/Application/Application.h>
#include <AzFramework/Windowing/NativeWindow.h>

#include <wayland-client.hpp>
#include <AzFramework/Protocols/XdgDecorManager.h>
#include <AzFramework/WaylandInputDeviceMouse.h>
#include <AzFramework/WaylandInputDeviceKeyboard.h>

namespace AzFramework
{
	class WaylandNativeWindow final
		: public NativeWindow::Implementation
	{
	public:
		AZ_CLASS_ALLOCATOR(WaylandNativeWindow, AZ::SystemAllocator);
		WaylandNativeWindow();
		~WaylandNativeWindow() override;

		void InitWindowInternal(const AZStd::string &title, const AzFramework::WindowGeometry &geometry, const AzFramework::WindowStyleMasks &styleMasks) override;
		NativeWindowHandle GetWindowHandle() const override;

		void SetWindowTitle(const AZStd::string &title) override;
		bool SupportsClientAreaResize() const override;

		float GetDpiScaleFactor() const override;
		uint32_t GetDisplayRefreshRate() const override;

		void UpdateRefreshRate(uint32_t newRefreshMhz);
		void UpdateScaleFactor(float newScale);

		bool GetFullScreenState() const override;
		void SetFullScreenState(bool fullScreenState) override;

		void SetPointerFocus(WaylandInputDeviceMouse* pointer);
		void SetKeyboardFocus(WaylandInputDeviceKeyboard* keyboard);

	private:
		static void SurfaceEnter(void *data,
								 struct wl_surface *wl_surface,
								 struct wl_output *output);

		static void SurfaceLeave(void *data,
								 struct wl_surface *wl_surface,
								 struct wl_output *output);

		static void SurfacePreferredScale(void *data,
										  struct wl_surface *wl_surface,
										  int32_t factor);

		static void SurfacePreferredTransform(void *data,
											  struct wl_surface *wl_surface,
											  uint32_t transform);

		const wl_surface_listener s_surfaceListener = {
			.enter = SurfaceEnter,
			.leave = SurfaceLeave,
			.preferred_buffer_scale = SurfacePreferredScale,
			.preferred_buffer_transform = SurfacePreferredTransform,
		};

		static void XdgSurfaceConfigure(void* data, struct xdg_surface* xdg_surface, uint32_t serial);

		const xdg_surface_listener s_xdgSurfaceListener = {
			.configure = XdgSurfaceConfigure
		};

		static void XdgTopLevelConfigure(void *,
										 struct xdg_toplevel *,
										 int32_t ,
										 int32_t ,
										 struct wl_array *);

		static void XdgTopLevelClose(void *data, struct xdg_toplevel *xdg_toplevel);

		static void XdgTopLevelConfigureBounds(void *,
											   struct xdg_toplevel *,
											   int32_t ,
											   int32_t );

		static void XdgTopLevelWmCaps(void *,
									  struct xdg_toplevel *,
									  struct wl_array *);

		const xdg_toplevel_listener s_xdgTopLevelListener = {
			.configure = XdgTopLevelConfigure,
			.close = XdgTopLevelClose,
			.configure_bounds = XdgTopLevelConfigureBounds,
			.wm_capabilities = XdgTopLevelWmCaps
		};

	private:
		//Globals cache
		wl_display* m_display;
		wl_compositor* m_compositor;
		xdg_wm_base* m_xdgShell;
		zxdg_decoration_manager_v1* m_xdgDecor;

		//Per window
		wl_surface* m_surface;
		xdg_surface* m_xdgSurface;
		xdg_toplevel* m_xdgToplevel;
		zxdg_toplevel_decoration_v1* m_xdgTopLevelDecor;

		wl_output* m_currentEnteredOutput = nullptr;
		wl_output* m_currentFullscreen = nullptr;

		WaylandInputDeviceMouse* m_focusedCursor = nullptr;
		WaylandInputDeviceKeyboard* m_focusedKeyboard = nullptr;

		//
		uint32_t m_currentRefreshMhz;
		uint32_t m_currentRefreshFramerate = 60;

		float m_dpiScaleFactor = 1.0f;
	};
}