/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <wayland-client.hpp>

#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AzFramework
{
    template<auto Callable>
    struct WaylandDeleterFreeFunctionWrapper
    {
        using value_type = decltype(Callable);
        static constexpr value_type s_value = Callable;
        constexpr operator value_type() const noexcept
        {
            return s_value;
        }
    };

    template<typename T, auto fn>
    using WaylandUniquePtr = AZStd::unique_ptr<T, WaylandDeleterFreeFunctionWrapper<fn>>;

    template<typename T>
    using WaylandStdFreePtr = WaylandUniquePtr<T, ::free>;

    // EBus to help with registry
    class WaylandRegistryEvents
    {
    public:
        AZ_RTTI(WaylandRegistryEvents, "{135E6733-E960-43B7-872C-C7B1E527D9B8}");

        virtual ~WaylandRegistryEvents() = default;

        virtual void OnRegister(wl_registry* registry, uint32_t id, const char* interface, uint32_t version) = 0;

        virtual void OnUnregister(wl_registry* registry, uint32_t id) = 0;
    };

    class WaylandRegistryEventsBusTraits : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    };

    using WaylandRegistryEventsBus = AZ::EBus<WaylandRegistryEvents, WaylandRegistryEventsBusTraits>;
    using WaylandRegistryEventsInterface = AZ::Interface<WaylandRegistryEvents>;

    class WaylandProtocolManagerBusTraits : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    };

    class WaylandInterfaceNotifications
    {
    public:
        AZ_RTTI(WaylandInterfaceNotifications, "{B8748E75-B6E0-48B3-95DC-26D24774E752}");

        virtual ~WaylandInterfaceNotifications() = default;

        virtual void OnProtocolError(uint32_t registryId, uint32_t errorCode) = 0;
    };

    class WaylandInterfaceNotificationsBusTraits : public AZ::EBusTraits
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;

        using BusIdType = uint32_t;
    };

    using WaylandInterfaceNotificationsBus = AZ::EBus<WaylandInterfaceNotifications, WaylandInterfaceNotificationsBusTraits>;
} // namespace AzFramework