/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/WaylandApplication.h>
#include <AzFramework/WaylandConnectionManager.h>
#include <AzFramework/WaylandInterface.h>
#include <AzFramework/Protocols/XdgDecorManager.h>
#include <AzFramework/Protocols/SeatManager.h>
#include <AzFramework/Protocols/CursorShapeManager.h>
#include <AzFramework/Protocols/PointerConstraintsManager.h>
#include <AzFramework/Protocols/RelativePointerManager.h>

#include <wayland-client.hpp>
#include <wayland-client-protocol.h>
#include <sys/poll.h>

#define IS_INTERFACE(wantedInter) strcmp(interface, wantedInter.name) == 0

namespace AzFramework
{
	struct WaylandSeat
	{
		WaylandSeat(wl_seat* inseat) : m_seat(inseat) {}

		wl_seat* m_seat;
        uint32_t m_registryId;
		uint32_t m_playerIdx;
		bool m_supportsPointer = false;
		bool m_supportsKeyboard = false;
		bool m_supportsTouch = false;

		AZStd::string m_name;
	};

	class WaylandConnectionManagerImpl
		: public WaylandConnectionManagerBus::Handler
		, public SeatManager
		, public CursorShapeManager
		, public PointerConstraintsManager
		, public RelativePointerManager
	{
	public:
		WaylandConnectionManagerImpl()
			: m_waylandDisplay(wl_display_connect(nullptr))
		{
			AZ_Error("Application", m_waylandDisplay != nullptr, "Unable to connect to Wayland Display.");
			m_fd = wl_display_get_fd(m_waylandDisplay.get());

			m_registry = wl_display_get_registry(m_waylandDisplay.get());
			AZ_Error("Application", m_registry != nullptr, "Unable to get Wayland Registry.");

			m_xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
			AZ_Error("Application", m_xkbContext != nullptr, "Unable to get XKB context.");

			wl_registry_add_listener(m_registry, &s_registry_listener, this);

			WaylandConnectionManagerBus::Handler::BusConnect();

			if(SeatManagerInterface::Get() == nullptr)
			{
				SeatManagerInterface::Register(this);
			}
		}

		~WaylandConnectionManagerImpl() override
		{
            {
                //GloablRegistryRemove will modify the vector
                //copy it for local
                auto seats = m_seats;
                for(auto& seat : seats)
                {
                    GlobalRegistryRemove(this, m_registry, seat.first);
                }
            }
            m_seats.clear();

			if(SeatManagerInterface::Get() == this)
			{
				SeatManagerInterface::Unregister(this);
			}
			if(CursorShapeManagerInterface::Get() == this)
			{
				CursorShapeManagerInterface::Unregister(this);
			}
			if(PointerConstraintsManagerInterface::Get() == this)
			{
				PointerConstraintsManagerInterface::Unregister(this);
			}
			if(RelativePointerManagerInterface::Get() == this)
			{
				RelativePointerManagerInterface::Unregister(this);
			}

			WaylandConnectionManagerBus::Handler::BusDisconnect();
			wl_registry_destroy(m_registry);
			wl_compositor_destroy(m_compositor);
		}

		void DoRoundtrip() const override{
			wl_display_roundtrip(m_waylandDisplay.get());
		}

        void CheckErrors() const override
        {
            int errorCode = wl_display_get_error(m_waylandDisplay.get());
            if (errorCode == EPROTO)
            {
                const wl_interface* anInterface;
                uint32_t interfaceId;
                auto code = wl_display_get_protocol_error(m_waylandDisplay.get(), &anInterface, &interfaceId);
                if(anInterface != nullptr)
                {
                    WaylandInterfaceNotificationsBus::Event(interfaceId, &WaylandInterfaceNotificationsBus::Events::OnProtocolError, interfaceId, code);
                }
            }

        }

		int GetDisplayFD() const override{
			return m_fd;
		}

		wl_display * GetWaylandDisplay() const override{
			return m_waylandDisplay.get();
		}

		wl_registry * GetWaylandRegistry() const override{
			return m_registry;
		}

		wl_compositor * GetWaylandCompositor() const override{
			return m_compositor;
		}

		xkb_context * GetXkbContext() const override{
			return m_xkbContext;
		}

		uint32_t GetSeatCount() const override{
			return m_seats.size();
		}

		wl_pointer * GetSeatPointer(uint32_t playerIdx) const override
		{
			if(auto seat = GetSeatFromPlayerIdx(playerIdx))
			{
				if(!seat->m_supportsPointer)
					return nullptr;
				return wl_seat_get_pointer(seat->m_seat);
			}
			return nullptr;
		}

		wl_keyboard * GetSeatKeyboard(uint32_t playerIdx) const override
		{
			if(auto seat = GetSeatFromPlayerIdx(playerIdx))
			{
				if(!seat->m_supportsKeyboard)
					return nullptr;
				return wl_seat_get_keyboard(seat->m_seat);
			}
			return nullptr;
		}

		wl_touch * GetSeatTouch(uint32_t playerIdx) const override
		{
			if(auto seat = GetSeatFromPlayerIdx(playerIdx))
			{
				if(!seat->m_supportsTouch)
					return nullptr;
				return wl_seat_get_touch(seat->m_seat);
			}
			return nullptr;
		}

		wp_cursor_shape_device_v1 * GetCursorShapeDevice(wl_pointer *pointer) override
		{
            if(m_cursorManager == nullptr)
            {
                return nullptr;
            }
			return wp_cursor_shape_manager_v1_get_pointer(m_cursorManager, pointer);
		}

		zwp_pointer_constraints_v1 * GetConstraints() override
		{
			return m_constraintsManager;
		}

		zwp_relative_pointer_v1 * GetRelativePointer(wl_pointer *pointer) override
		{
			if(m_relativePointerManager == nullptr)
			{
				return nullptr;
			}
			return zwp_relative_pointer_manager_v1_get_relative_pointer(m_relativePointerManager, pointer);
		}

		static void GlobalRegistryHandler(void* data,
										  wl_registry* registry,
										  uint32_t id,
										  const char* interface,
										  uint32_t version)
		{
			auto self = static_cast<WaylandConnectionManagerImpl*>(data);

			if(IS_INTERFACE(wl_compositor_interface))
			{
				self->m_compositor =
					static_cast<wl_compositor*>(wl_registry_bind(registry, id, &wl_compositor_interface, version));
                self->m_compositorId = id;
			}
			else if (IS_INTERFACE(wl_seat_interface))
			{
				auto seat =
					static_cast<wl_seat*>(wl_registry_bind(registry, id, &wl_seat_interface, version));
				auto info = new WaylandSeat(seat);
                info->m_registryId = id;
				info->m_playerIdx = self->GetAvailablePlayerIdx();
				wl_seat_add_listener(seat, &self->s_seat_listener, info);

				self->m_seats.emplace(id, info);
			}
			else if (IS_INTERFACE(wp_cursor_shape_manager_v1_interface))
			{
				self->m_cursorManager =
					static_cast<wp_cursor_shape_manager_v1*>(wl_registry_bind(registry, id, &wp_cursor_shape_manager_v1_interface, version));
                self->m_cursorManagerId = id;

                if(CursorShapeManagerInterface::Get() == nullptr)
				{
                    CursorShapeManagerInterface::Register(self);
                }
			}
			else if (IS_INTERFACE(zwp_pointer_constraints_v1_interface))
			{
				self->m_constraintsManager =
					static_cast<zwp_pointer_constraints_v1*>(wl_registry_bind(registry, id, &zwp_pointer_constraints_v1_interface, version));
                self->m_constraintsManagerId = id;

                if(PointerConstraintsManagerInterface::Get() == nullptr)
				{
                    PointerConstraintsManagerInterface::Register(self);
                }
			}
			else if (IS_INTERFACE(zwp_relative_pointer_manager_v1_interface))
			{
				self->m_relativePointerManager =
					static_cast<zwp_relative_pointer_manager_v1*>(wl_registry_bind(registry, id, &zwp_relative_pointer_manager_v1_interface, version));
                self->m_relativePointerManagerId = id;

                if(RelativePointerManagerInterface::Get() == nullptr)
				{
                    RelativePointerManagerInterface::Register(self);
                }
            }
			else
			{
				WaylandRegistryEventsBus::Broadcast(
					&WaylandRegistryEventsBus::Events::OnRegister,
					registry,
					id,
					interface,
					version);
			}
		}

		static void GlobalRegistryRemove(void* data, wl_registry* registry, uint32_t id){
			auto self = static_cast<WaylandConnectionManagerImpl*>(data);
			if(self->m_compositorId == id){
				//OH GOD OH NO!
			}
			else if (self->m_seats.find(id) != self->m_seats.end())
			{
				//it be a seat
				auto seat = self->m_seats[id];

                //Tell people using this seat to release any wl resources.
			    AzFramework::SeatNotificationsBus::Event(
                        seat->m_playerIdx,
                        &AzFramework::SeatNotificationsBus::Events::ReleaseSeat);

				self->m_seats.erase(id);

				wl_seat_destroy(seat->m_seat);
				delete seat;
			}
			else if (self->m_cursorManagerId == id)
			{
				wp_cursor_shape_manager_v1_destroy(self->m_cursorManager);
				self->m_cursorManager = nullptr;
                self->m_cursorManagerId = 0;

                if(CursorShapeManagerInterface::Get() == self)
				{
                    CursorShapeManagerInterface::Unregister(self);
                }
			}
			else if (self->m_constraintsManagerId == id)
			{
				zwp_pointer_constraints_v1_destroy(self->m_constraintsManager);
				self->m_constraintsManager = nullptr;
                self->m_constraintsManagerId = 0;

                if(PointerConstraintsManagerInterface::Get() == self)
				{
                    PointerConstraintsManagerInterface::Unregister(self);
                }
			}
			else if (self->m_relativePointerManagerId == id)
			{
				zwp_relative_pointer_manager_v1_destroy(self->m_relativePointerManager);
				self->m_relativePointerManager = nullptr;
                self->m_relativePointerManagerId = 0;

                if(RelativePointerManagerInterface::Get() == self)
				{
                    RelativePointerManagerInterface::Unregister(self);
                }
			}
			else
			{
				WaylandRegistryEventsBus::Broadcast(
					&WaylandRegistryEventsBus::Events::OnUnregister,
					registry,
					id);
			}
		}

		static void SeatCaps(void *data,
							 struct wl_seat *wl_seat,
							 uint32_t capabilities)
		{
			auto self = static_cast<WaylandSeat*>(data);

			self->m_supportsPointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
			self->m_supportsKeyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
			self->m_supportsTouch = capabilities & WL_SEAT_CAPABILITY_TOUCH;

			//Tell people that the caps have changed
			AzFramework::SeatNotificationsBus::Event(self->m_playerIdx, &AzFramework::SeatNotificationsBus::Events::SeatCapsChanged);
		}

		static void SeatName(void *data,
							 struct wl_seat *wl_seat,
							 const char *name)
		{
			auto self = static_cast<WaylandSeat*>(data);
			self->m_name = AZStd::string (name);
		}


	private:
		WaylandSeat* GetSeatFromPlayerIdx(uint32_t playerIdx) const
		{
			for (auto& seat : m_seats)
			{
				if(seat.second->m_playerIdx == playerIdx)
				{
					return seat.second;
				}
			}

			return nullptr;
		}

		uint32_t GetAvailablePlayerIdx() const
		{
			for (uint32_t i = 0; i < UINT32_MAX; ++i)
			{
				if(GetSeatFromPlayerIdx(i) == nullptr)
				{
					return i;
				}
			}

			return UINT32_MAX;
		}

		int m_fd = -1;
		WaylandUniquePtr<wl_display, wl_display_disconnect> m_waylandDisplay = nullptr;
		wl_registry* m_registry = nullptr;
		wl_compositor* m_compositor = nullptr;
        uint32_t m_compositorId = 0;
		xkb_context* m_xkbContext = nullptr;
		wp_cursor_shape_manager_v1* m_cursorManager = nullptr;
        uint32_t m_cursorManagerId = 0;
		zwp_pointer_constraints_v1* m_constraintsManager = nullptr;
        uint32_t m_constraintsManagerId = 0;
		zwp_relative_pointer_manager_v1* m_relativePointerManager = nullptr;
        uint32_t m_relativePointerManagerId = 0;

		//Registry id -> WaylandSeat Ptr
		AZStd::map<uint32_t, WaylandSeat*> m_seats;

		const wl_registry_listener s_registry_listener = {
			.global = GlobalRegistryHandler,
			.global_remove = GlobalRegistryRemove
		};

		const wl_seat_listener s_seat_listener = {
			.capabilities = SeatCaps,
			.name = SeatName
		};

	};

	class XdgManagerImpl
		: public XdgShellConnectionManager
		, public XdgDecorConnectionManager
		, public WaylandRegistryEventsBus::Handler
        , public WaylandInterfaceNotificationsBus::MultiHandler
	{
	public:
		XdgManagerImpl(){
			WaylandRegistryEventsBus::Handler::BusConnect();

			if(XdgShellConnectionManagerInterface::Get() == nullptr)
			{
				XdgShellConnectionManagerInterface::Register(this);
			}

			if(XdgDecorConnectionManagerInterface::Get() == nullptr)
			{
				XdgDecorConnectionManagerInterface::Register(this);
			}
		}

		~XdgManagerImpl(){
			WaylandRegistryEventsBus::Handler::BusDisconnect();

			if(XdgShellConnectionManagerInterface::Get() == this)
			{
				XdgShellConnectionManagerInterface::Unregister(this);
			}
			if(XdgDecorConnectionManagerInterface::Get() == this)
			{
				XdgDecorConnectionManagerInterface::Unregister(this);
			}

			xdg_wm_base_destroy(m_xdg);
			zxdg_decoration_manager_v1_destroy(m_decor);
		}

        uint32_t GetXdgWmBaseRegistryId() const override
        {
            return m_xdgId;
        }

		xdg_wm_base * GetXdgWmBase() const override
        {
			return m_xdg;
		}

		zxdg_decoration_manager_v1 * GetXdgDecor() const override
        {
			return m_decor;
		}

		void OnRegister(wl_registry *registry, uint32_t id, const char *interface, uint32_t version) override {
			if(IS_INTERFACE(xdg_wm_base_interface))
			{
				m_xdg =
					static_cast<xdg_wm_base*>(wl_registry_bind(registry, id, &xdg_wm_base_interface, version));
				xdg_wm_base_add_listener(m_xdg, &s_xdg_wm_listener, this);
                m_xdgId = id;
                WaylandInterfaceNotificationsBus::MultiHandler::BusConnect(m_xdgId);
			}
			else if(IS_INTERFACE(zxdg_decoration_manager_v1_interface))
			{
				m_decor =
					static_cast<zxdg_decoration_manager_v1*>(wl_registry_bind(registry, id, &zxdg_decoration_manager_v1_interface, version));
                m_decorId = id;
                WaylandInterfaceNotificationsBus::MultiHandler::BusConnect(m_decorId);
			}
		}

		void OnUnregister(wl_registry *, uint32_t id) override{
            if(m_xdgId == id)
            {
                WaylandInterfaceNotificationsBus::MultiHandler::BusDisconnect(m_xdgId);
                m_xdgId = 0;
                xdg_wm_base_destroy(m_xdg);
                m_xdg = nullptr;
            }
			else if(m_decorId == id)
            {
                WaylandInterfaceNotificationsBus::MultiHandler::BusDisconnect(m_decorId);
                m_decorId = 0;
                zxdg_decoration_manager_v1_destroy(m_decor);
                m_decor = nullptr;
            }
		}

        void OnProtocolError(uint32_t registryId, uint32_t errorCode) override
        {
            auto conn = WaylandConnectionManagerInterface::Get();
            if(!conn)
            {
                return;
            }
            if(m_xdgId == registryId)
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
			else if(m_decorId == registryId)
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

		static void XdgPing(void* data, xdg_wm_base* xdg, uint32_t serial)
        {
			//You ping, I pong :)
			xdg_wm_base_pong(xdg, serial);
		}

		const xdg_wm_base_listener s_xdg_wm_listener = {
			.ping = XdgPing
		};


	private:
		xdg_wm_base* m_xdg;
        uint32_t m_xdgId;
		zxdg_decoration_manager_v1* m_decor;
        uint32_t m_decorId;
	};

	struct OutputInfo {
		wl_output* m_output;
		uint32_t m_id;
		bool m_isDone = false;
		int32_t m_x;
		int32_t m_y;
		int32_t m_width;
		int32_t m_height;
		int32_t m_refreshRateMhz;
		int32_t m_physicalWidth;
		int32_t m_physicalHeight;
		wl_output_subpixel m_subpixel;
		AZStd::string m_make;
		AZStd::string m_model;
		wl_output_transform m_transform;

		AZStd::string m_name;
		AZStd::string m_desc;

		int32_t m_scaleFactor;
	};

	class OutputManagerImpl
		: public OutputManager
		, public WaylandRegistryEventsBus::Handler
	{
	public:
		OutputManagerImpl()
		{
			WaylandRegistryEventsBus::Handler::BusConnect();

			if(OutputManagerInterface::Get() == nullptr)
			{
				OutputManagerInterface::Register(this);
			}
		}

		~OutputManagerImpl() override
		{
			WaylandRegistryEventsBus::Handler::BusDisconnect();

			if(OutputManagerInterface::Get() == this)
			{
				OutputManagerInterface::Unregister(this);
			}
		}

		uint32_t GetRefreshRateMhz(wl_output *output) override
		{
			OutputInfo* info = static_cast<OutputInfo *>(wl_output_get_user_data(output));
			if(info == nullptr || !info->m_isDone)
				return 0;

			return info->m_refreshRateMhz;
		}

		AZStd::string GetOutputName(wl_output *output) override
		{
			OutputInfo* info = static_cast<OutputInfo *>(wl_output_get_user_data(output));
			if(info == nullptr || !info->m_isDone)
				return {};

			return info->m_name;
		}

		AZStd::string GetOutputDesc(wl_output *output) override
		{
			OutputInfo* info = static_cast<OutputInfo *>(wl_output_get_user_data(output));
			if(info == nullptr || !info->m_isDone)
				return {};

			return info->m_desc;
		}

		void OnRegister(wl_registry *registry, uint32_t id, const char *interface, uint32_t version) override
		{
			if(!IS_INTERFACE(wl_output_interface))
			{
				return;
			}

			wl_output* output =
				static_cast<wl_output*>(wl_registry_bind(registry, id, &wl_output_interface, version));

			auto info = azcreate(OutputInfo);
			info->m_output = output;
			info->m_id = id;

			wl_output_set_user_data(output, info);

			wl_output_add_listener(output, &s_output_listener, info);

			m_outputs.emplace(id, info);
		}

		void OnUnregister(wl_registry *registry, uint32_t id) override{
			if(m_outputs.find(id) == m_outputs.end())
			{
				return;
			}

			auto info = m_outputs[id];
			m_outputs.erase(id);

			wl_output_destroy(info->m_output);

			azdestroy(info);
		}

		static void OutputGeometry(void *data,
								   struct wl_output *wl_output,
								   int32_t x,
								   int32_t y,
								   int32_t physical_width,
								   int32_t physical_height,
								   int32_t subpixel,
								   const char *make,
								   const char *model,
								   int32_t transform)
		{
			auto self = static_cast<OutputInfo*>(data);
			self->m_x = x;
			self->m_y = y;
			self->m_physicalWidth = physical_width;
			self->m_physicalHeight = physical_height;
			self->m_subpixel = static_cast<wl_output_subpixel>(subpixel);
			self->m_make = AZStd::string(make);
			self->m_model = AZStd::string(model);
			self->m_transform = static_cast<wl_output_transform>(transform);
		}

		static void OutputMode(
			void *data,
			struct wl_output *wl_output,
			uint32_t flags,
			int32_t width,
			int32_t height,
			int32_t refresh)
		{
			auto self = static_cast<OutputInfo*>(data);
			if(flags & wl_output_mode::WL_OUTPUT_MODE_CURRENT)
			{
				//We only really care for the current mode.
				self->m_width = width;
				self->m_height = height;
				self->m_refreshRateMhz = refresh;
			}
		}

		static void OutputDone(void *data, struct wl_output *wl_output)
		{
			auto self = static_cast<OutputInfo*>(data);
			self->m_isDone = true;
		}

		static void OutputScale(void *data, struct wl_output *wl_output, int32_t factor)
		{
			auto self = static_cast<OutputInfo*>(data);
			self->m_scaleFactor = factor;
		}

		static void OutputName(void *data, struct wl_output *wl_output, const char* name)
		{
			auto self = static_cast<OutputInfo*>(data);
			self->m_name = AZStd::string(name);
		}

		static void OutputDesc(void *data, struct wl_output *wl_output, const char* description)
		{
			auto self = static_cast<OutputInfo*>(data);
			self->m_desc = AZStd::string(description);
		}

		const wl_output_listener s_output_listener = {
			.geometry = OutputGeometry,
			.mode = OutputMode,
			.done = OutputDone,
			.scale = OutputScale,
			.name = OutputName,
			.description = OutputDesc
		};

	private:
		AZStd::map<uint32_t, OutputInfo*> m_outputs;
	};

	WaylandApplication::WaylandApplication()
	{
		LinuxLifecycleEvents::Bus::Handler::BusConnect();

		m_waylandConnectionManager = AZStd::make_unique<WaylandConnectionManagerImpl>();
		if(WaylandConnectionManagerInterface::Get() == nullptr)
		{
			WaylandConnectionManagerInterface::Register(m_waylandConnectionManager.get());
		}

		//Add needed protocols
		m_outputManager = AZStd::make_unique<OutputManagerImpl>();
		m_xdgManager = AZStd::make_unique<XdgManagerImpl>();

		WaylandConnectionManagerBus::Broadcast(&WaylandConnectionManagerBus::Events::DoRoundtrip);
		PumpSystemEventLoopOnce();

	}

	WaylandApplication::~WaylandApplication()
	{
		if(WaylandConnectionManagerInterface::Get() == m_waylandConnectionManager.get())
		{
			WaylandConnectionManagerInterface::Unregister(m_waylandConnectionManager.get());
		}
		m_outputManager.reset();
		m_xdgManager.reset();
		m_waylandConnectionManager.reset();
		LinuxLifecycleEvents::Bus::Handler::BusDisconnect();
	}


	bool WaylandApplication::HasEventsWaiting()
	{
		int fd = m_waylandConnectionManager->GetDisplayFD();
		struct pollfd pfd = {fd, POLLIN};

		return poll(&pfd, 1, 0) > 0;
	}

	void WaylandApplication::PumpSystemEventLoopOnce()
	{
		if(wl_display* display = m_waylandConnectionManager->GetWaylandDisplay())
        {
			if(wl_display_dispatch_pending(display) == 0)
            {
				//no pending events, read new events
				wl_display_flush(display);
				wl_display_prepare_read(display);

				if(HasEventsWaiting())
                {
					wl_display_read_events(display);
					wl_display_dispatch_pending(display);
				}
				else
                {
					wl_display_cancel_read(display);
				}
			}
		}
        m_waylandConnectionManager->CheckErrors();
	}

	void WaylandApplication::PumpSystemEventLoopUntilEmpty()
	{
		if(wl_display* display = m_waylandConnectionManager->GetWaylandDisplay())
        {
			while (true)
            {
				if (wl_display_dispatch_pending(display) == 0)
                {
					//no pending events, read new events
					wl_display_flush(display);
					wl_display_prepare_read(display);

					if (HasEventsWaiting())
                    {
						wl_display_read_events(display);
						wl_display_dispatch_pending(display);
					}
					else
                    {
						wl_display_cancel_read(display);
						break; //no events are pending
					}
				}
			}
		}
        m_waylandConnectionManager->CheckErrors();
	}
}