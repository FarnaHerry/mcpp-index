// Behavioral test — verify compat.eui-neo builds, exposes its umbrella header
// to a Form A consumer, and links against real symbols from the built lib.
//
// `#include <eui_neo.h>` is the whole point of the header-compat shape. The
// umbrella declares the DSL app surface (eui/dsl_app.h) and leaves two symbols
// for the application to supply — app::dslAppConfig() and app::compose().
// Since 0.5.6 the umbrella no longer pulls in eui/detail/dsl_app_impl.h (the
// impl lives in the app-main entry points instead), but THIS TU only defines
// the two app symbols and never calls app::initialize/update/render — so it
// links without the impl header, and defining the app symbols is exactly what
// upstream's examples/*.cpp do, making this test a faithful minimal consumer.
//
// The assertion itself runs on eui::json::Document: it lives in
// core/platform/json.cpp, so a package that compiled zero translation units
// fails at LINK time instead of silently passing — which is how an earlier
// revision of this descriptor shipped an empty lib behind a green CI. It is
// also the cheapest entry point that is genuinely headless (no window, no GL
// context), which matters because CI runners have no display.
#include <eui_neo.h>
import std;

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("compat.eui-neo smoke test")
        .windowSize(320, 240);
    return config;
}

// Never invoked: the test asserts on the headless JSON facade instead of
// entering the render loop. It exists so the DSL app skeleton links.
void compose(eui::Ui&, const eui::Screen&) {}

} // namespace app

int main() {
    // The umbrella header really did reach us, with the DSL app config intact.
    if (app::dslAppConfig().windowWidthValue != 320) {
        std::println("dslAppConfig() not wired: width={}", app::dslAppConfig().windowWidthValue);
        return 1;
    }

    eui::json::Document doc;
    if (!doc.parse(R"({"framework": {"name": "eui-neo", "version": 3}})")) {
        std::println("parse failed: {}", doc.error().message);
        return 2;
    }

    std::string name;
    if (!doc.stringAt("/framework/name", name) || name != "eui-neo") {
        std::println("unexpected /framework/name: '{}'", name);
        return 3;
    }

    // number() rather than signedInteger(): yyjson tags a positive literal as
    // an UNSIGNED integer, so yyjson_is_sint() — and therefore
    // Value::signedInteger() — is false for `3`.
    double version = 0.0;
    if (!doc.atPointer("/framework/version").number(version) || version != 3.0) {
        std::println("unexpected /framework/version: {}", version);
        return 4;
    }

    // Forces core/platform/platform.cpp into the link. That TU emits
    // `platform.o`, which collides with compat.glfw's src/platform.c in mcpp's
    // flat per-link obj/ directory (mcpp#233/#240) and used to be dropped
    // silently — the descriptor now routes it through a uniquely named
    // generated stub. Referencing a symbol only that TU defines turns a
    // regression into an undefined reference instead of a passing test.
    // Read-only on purpose: requestFrame() would reach glfwPostEmptyEvent()
    // with no window, and CI runners are headless. Both flags start clear.
    if (core::platform::consumeFrameRequest() || core::platform::consumeUiUpdate()) {
        std::println("platform frame flags did not start clear");
        return 5;
    }

    // NEGATIVE verification for the `markdown` feature, which is NOT requested
    // by this member. components/markdown.h ships two definitions of
    // parseMarkdownBlocks: the md4c one, and — when EUI_HAS_MD4C is absent — a
    // degenerate fallback that wraps the entire source in a single Paragraph.
    // Getting the fallback here proves the feature's interface define really is
    // gated off by default. tests/examples/eui-neo-markdown asserts the other
    // side of the same switch.
    const auto blocks = components::detail::parseMarkdownBlocks("# Heading\n\nBody text.\n");
    if (blocks.size() != 1 || blocks[0].kind != components::detail::MarkdownBlockKind::Paragraph) {
        std::println("markdown feature leaked into the default build: {} block(s)", blocks.size());
        return 6;
    }

    std::println("compat.eui-neo smoke test: ok (parsed {} v{}, markdown gated off)", name, version);
    return 0;
}
