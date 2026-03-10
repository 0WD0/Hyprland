#pragma once

#include <vector>
#include "WaylandProtocol.hpp"
#include "gtk-shell.hpp"

class CGTKSurfaceResource {
  public:
    CGTKSurfaceResource(SP<CGtkSurface1> resource_, wl_resource* surface_);

    bool good();

  private:
    void handleFocusRequest(const char* requestName);

    SP<CGtkSurface1> m_resource;
    wl_resource*     m_surface = nullptr;
    bool             m_modal   = false;

    friend class CGTKShellProtocol;
};

class CGTKShellManagerResource {
  public:
    CGTKShellManagerResource(UP<CGtkShell1>&& resource);

    bool good();

  private:
    UP<CGtkShell1> m_resource;

    friend class CGTKShellProtocol;
};

class CGTKShellProtocol : public IWaylandProtocol {
  public:
    CGTKShellProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyManager(CGTKShellManagerResource* res);
    void destroyGtkSurface(CGTKSurfaceResource* res);

    std::vector<UP<CGTKShellManagerResource>> m_managers;
    std::vector<SP<CGTKSurfaceResource>>      m_surfaces;

    friend class CGTKShellManagerResource;
    friend class CGTKSurfaceResource;
};

namespace PROTO {
    inline UP<CGTKShellProtocol> gtkShell;
};
