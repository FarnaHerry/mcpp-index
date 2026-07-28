// Smoke test — verify compat.yyjson builds, links, and works
#include "yyjson.h"
import std;

int main() {
    const char* json = R"({"name":"mcpp","version":"0.12.0"})";
    yyjson_doc* doc = yyjson_read(json, strlen(json), 0);
    if (!doc) return 1;
    auto* root = yyjson_doc_get_root(doc);
    auto* name = yyjson_obj_get(root, "name");
    if (!name || std::string_view{yyjson_get_str(name)} != "mcpp") return 2;
    std::println("compat.yyjson smoke test: ok");
    yyjson_doc_free(doc);
    return 0;
}
