// POSITIVE verification for compat.eui-neo's `markdown` feature.
//
// components/markdown.h is header-only and compiles one of two definitions of
// detail::parseMarkdownBlocks depending on EUI_HAS_MD4C. That makes it a clean
// probe for something the descriptor cannot otherwise prove: the feature's
// `defines` really do reach the CONSUMER's translation unit, not just the
// package's own. Without propagation this member would link md4c and still
// silently get the fallback parser.
//
// tests/examples/eui-neo asserts the negative side — same header, feature off,
// degenerate single-Paragraph result.
#include <eui_neo.h>
import std;

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}.title("markdown feature test");
    return config;
}

void compose(eui::Ui&, const eui::Screen&) {}

} // namespace app

int main() {
#if !defined(EUI_HAS_MD4C)
    std::println("EUI_HAS_MD4C not defined — the feature's interface define did not propagate");
    return 1;
#else
    namespace md = components::detail;

    const auto blocks = md::parseMarkdownBlocks("# Heading\n\nBody text.\n");

    // The real parser splits heading from paragraph; the fallback returns one
    // Paragraph holding the raw source.
    if (blocks.size() < 2) {
        std::println("expected >= 2 blocks from the md4c parser, got {}", blocks.size());
        return 2;
    }
    if (blocks[0].kind != md::MarkdownBlockKind::Heading || blocks[0].headingLevel != 1) {
        std::println("expected an h1 first block, got kind={} level={}",
                     static_cast<int>(blocks[0].kind), blocks[0].headingLevel);
        return 3;
    }
    if (md::plainText(blocks[0].runs) != "Heading") {
        std::println("unexpected heading text: '{}'", md::plainText(blocks[0].runs));
        return 4;
    }

    std::println("compat.eui-neo[markdown]: ok ({} blocks, h1 = '{}')",
                 blocks.size(), md::plainText(blocks[0].runs));
    return 0;
#endif
}
