#include "GTKShell.hpp"

#include "core/Compositor.hpp"
#include "../Compositor.hpp"
#include "../desktop/view/Window.hpp"
#include "../managers/EventManager.hpp"

#include <algorithm>

CGTKSurfaceResource::CGTKSurfaceResource(SP<CGtkSurface1> resource_, wl_resource* surface_) : m_resource(resource_), m_surface(surface_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setOnDestroy([this](CGtkSurface1* r) { PROTO::gtkShell->destroyGtkSurface(this); });
    m_resource->setRelease([this](CGtkSurface1* r) { PROTO::gtkShell->destroyGtkSurface(this); });

    m_resource->setSetDbusProperties([this](CGtkSurface1* r, const char* applicationID, const char* appMenuPath, const char* menubarPath, const char* windowObjectPath,
                                            const char* applicationObjectPath, const char* uniqueBusName) {
        LOGM(Log::TRACE,
             "gtk_surface1.set_dbus_properties on {:x} (app-id {}, app-menu {}, menubar {}, window-path {}, app-path {}, bus {})",
             (uintptr_t)m_surface,
             applicationID ? applicationID : "",
             appMenuPath ? appMenuPath : "",
             menubarPath ? menubarPath : "",
             windowObjectPath ? windowObjectPath : "",
             applicationObjectPath ? applicationObjectPath : "",
             uniqueBusName ? uniqueBusName : "");
    });

    m_resource->setSetModal([this](CGtkSurface1* r) {
        m_modal = true;
        LOGM(Log::TRACE, "gtk_surface1.set_modal on {:x}", (uintptr_t)m_surface);
    });

    m_resource->setUnsetModal([this](CGtkSurface1* r) {
        m_modal = false;
        LOGM(Log::TRACE, "gtk_surface1.unset_modal on {:x}", (uintptr_t)m_surface);
    });

    m_resource->setPresent([this](CGtkSurface1* r, uint32_t time) {
        LOGM(Log::TRACE, "gtk_surface1.present on {:x} (time {})", (uintptr_t)m_surface, time);
        handleFocusRequest("gtk_surface1.present");
    });

    m_resource->setRequestFocus([this](CGtkSurface1* r, const char* startupID) {
        LOGM(Log::TRACE, "gtk_surface1.request_focus on {:x} (startup-id {})", (uintptr_t)m_surface, startupID ? startupID : "");
        handleFocusRequest("gtk_surface1.request_focus");
    });

    m_resource->setTitlebarGesture([this](CGtkSurface1* r, uint32_t gesture, wl_resource* seat, uint32_t serial) {
        LOGM(Log::TRACE, "gtk_surface1.titlebar_gesture on {:x} (gesture {}, seat {:x}, serial {})", (uintptr_t)m_surface, gesture, (uintptr_t)seat, serial);
    });

    wl_array empty;
    wl_array_init(&empty);
    m_resource->sendConfigure(&empty);
    if (m_resource->version() >= 2)
        m_resource->sendConfigureEdges(&empty);
    wl_array_release(&empty);
}

bool CGTKSurfaceResource::good() {
    return m_resource->resource();
}

void CGTKSurfaceResource::handleFocusRequest(const char* requestName) {
    const auto SURFACE = CWLSurfaceResource::fromResource(m_surface);

    if UNLIKELY (!SURFACE) {
        LOGM(Log::WARN, "{} for dead surface {:x}, ignoring", requestName, (uintptr_t)m_surface);
        return;
    }

    const auto PWINDOW = g_pCompositor->getWindowFromSurface(SURFACE);

    if UNLIKELY (!PWINDOW) {
        LOGM(Log::WARN, "{} for non-window surface {:x}, ignoring", requestName, (uintptr_t)m_surface);
        return;
    }

    LOGM(Log::DEBUG, "{} focusing {}", requestName, PWINDOW);
    PWINDOW->activate(true);
}

CGTKShellManagerResource::CGTKShellManagerResource(UP<CGtkShell1>&& resource) : m_resource(std::move(resource)) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CGtkShell1* r) { PROTO::gtkShell->destroyManager(this); });

    m_resource->setGetGtkSurface([this](CGtkShell1* r, uint32_t id, wl_resource* surface) {
        LOGM(Log::TRACE, "gtk_shell1.get_gtk_surface for surface {:x}", (uintptr_t)surface);

        const auto RESOURCE = PROTO::gtkShell->m_surfaces.emplace_back(makeShared<CGTKSurfaceResource>(makeShared<CGtkSurface1>(r->client(), r->version(), id), surface));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::gtkShell->m_surfaces.pop_back();
            return;
        }
    });

    m_resource->setSetStartupId([](CGtkShell1* r, const char* startupID) {
        LOGM(Log::TRACE, "gtk_shell1.set_startup_id {}", startupID ? startupID : "");
    });

    m_resource->setSystemBell([](CGtkShell1* r, wl_resource* surface) {
        LOGM(Log::TRACE, "gtk_shell1.system_bell on {:x}", (uintptr_t)surface);

        if (!surface) {
            g_pEventManager->postEvent(SHyprIPCEvent{.event = "bell", .data = ""});
            return;
        }

        const auto SURFACE = CWLSurfaceResource::fromResource(surface);

        if (!SURFACE) {
            g_pEventManager->postEvent(SHyprIPCEvent{.event = "bell", .data = ""});
            return;
        }

        for (const auto& w : g_pCompositor->m_windows) {
            if (!w->m_isMapped || w->m_isX11 || !w->m_xdgSurface || !w->wlSurface())
                continue;

            if (w->wlSurface()->resource() == SURFACE) {
                g_pEventManager->postEvent(SHyprIPCEvent{.event = "bell", .data = std::format("{:x}", rc<uintptr_t>(w.get()))});
                return;
            }
        }

        g_pEventManager->postEvent(SHyprIPCEvent{.event = "bell", .data = ""});
    });

    m_resource->setNotifyLaunch([](CGtkShell1* r, const char* startupID) {
        LOGM(Log::TRACE, "gtk_shell1.notify_launch {}", startupID ? startupID : "");
    });

    m_resource->sendCapabilities(0);
}

bool CGTKShellManagerResource::good() {
    return m_resource->resource();
}

CGTKShellProtocol::CGTKShellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CGTKShellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = WP<CGTKShellManagerResource>{m_managers.emplace_back(makeUnique<CGTKShellManagerResource>(makeUnique<CGtkShell1>(client, ver, id)))};

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        return;
    }
}

void CGTKShellProtocol::destroyManager(CGTKShellManagerResource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == res; });
}

void CGTKShellProtocol::destroyGtkSurface(CGTKSurfaceResource* res) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == res; });
}
