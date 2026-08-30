// FTXUI 7 module smoke: compat.ftxui 7.0.3 compiles upstream's own .cppm
// units, so consumers can `import ftxui;` instead of (or mixed with)
// #include — the re-export style keeps every declaration in the global
// module, so both spellings link against the same objects.
import ftxui;

#include <string>

#include <gtest/gtest.h>

TEST(CompatModule, FtxuiImport) {
    using namespace ftxui;
    Element document = hbox({text("module"), separator(), text("ftxui")});
    Screen screen = Screen::Create(Dimension::Fit(document), Dimension::Fit(document));
    Render(screen, document);
    const std::string rendered = screen.ToString();
    EXPECT_TRUE(rendered.find("module") != std::string::npos &&
                rendered.find("ftxui") != std::string::npos);
}
