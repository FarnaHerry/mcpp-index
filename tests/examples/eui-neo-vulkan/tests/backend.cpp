// Verify compat.eui-neo's `vulkan` feature actually swaps the render backend.
//
// Two things have to hold, and only one of them is about Vulkan:
//
//  1. The backend define really flipped. EUI_RENDER_BACKEND_VULKAN must be
//     visible here and EUI_RENDER_BACKEND_OPENGL must NOT — if `default` had
//     leaked through alongside the explicit feature, both would be set and
//     core/render/render_backend.cpp would have silently compiled the OpenGL
//     path (its dispatch is `#if OPENGL … #elif VULKAN`, so OpenGL wins).
//     That failure is invisible at runtime without a GPU, so it is asserted at
//     COMPILE time below.
//
//  2. The Vulkan translation units are linked. compat.vulkan's loader entry
//     points must resolve from a consumer that reached them through eui-neo's
//     feature-scoped dependency, not through a direct one.
//
// Everything else about the Vulkan backend needs a device, a surface and a
// window, none of which exist on a CI runner — see the design doc.
//
// HAVE_EUI_VULKAN is set by THIS project's own [target.'cfg(...)'.build]
// cxxflags: compat.vulkan has no windows build, so on windows the member takes
// the default OpenGL configuration instead and asserts THAT.
#include <eui_neo.h>
#if defined(HAVE_EUI_VULKAN)
#include <vulkan/vulkan.h>
#endif
import std;

#if defined(HAVE_EUI_VULKAN)
#  if !defined(EUI_RENDER_BACKEND_VULKAN)
#    error "vulkan feature requested but EUI_RENDER_BACKEND_VULKAN is not defined"
#  endif
#  if defined(EUI_RENDER_BACKEND_OPENGL)
#    error "EUI_RENDER_BACKEND_OPENGL leaked in alongside the vulkan feature"
#  endif
#else
// The default (OpenGL) build publishes NO interface define — the package
// resolves it in its own preprocessor, which the consumer never sees. So the
// only thing assertable from here is the absence of the vulkan one.
#  if defined(EUI_RENDER_BACKEND_VULKAN)
#    error "EUI_RENDER_BACKEND_VULKAN present without the vulkan feature"
#  endif
#endif

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}.title("vulkan backend test");
    return config;
}

void compose(eui::Ui&, const eui::Screen&) {}

} // namespace app

int main() {
    // The loader answers this without any ICD, so it is safe on a driverless
    // runner while still proving compat.vulkan came along with the feature.
#if defined(HAVE_EUI_VULKAN)
    std::uint32_t apiVersion = 0;
    if (vkEnumerateInstanceVersion(&apiVersion) != VK_SUCCESS) {
        std::println("vkEnumerateInstanceVersion failed");
        return 1;
    }
#endif

    // Still the eui-neo library underneath: assert on a headless facade that
    // lives in core/platform/json.cpp, so a build that dropped the base
    // sources while chasing the feature fails here.
    eui::json::Document doc;
    if (!doc.parse(R"({"backend": "vulkan"})")) {
        std::println("parse failed: {}", doc.error().message);
        return 2;
    }
    std::string backend;
    if (!doc.stringAt("/backend", backend) || backend != "vulkan") {
        std::println("unexpected /backend: '{}'", backend);
        return 3;
    }

#if defined(HAVE_EUI_VULKAN)
    std::println("compat.eui-neo[vulkan]: ok (backend={}, loader api {}.{}.{})",
                 backend, VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion),
                 VK_VERSION_PATCH(apiVersion));
#else
    std::println("compat.eui-neo[vulkan]: skipped, default opengl build asserted instead (parsed {})",
                 backend);
#endif
    return 0;
}
