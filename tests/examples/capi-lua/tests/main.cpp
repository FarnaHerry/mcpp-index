import mcpplibs.capi.lua;

namespace lua = mcpplibs::capi::lua;

int main() {
    auto* state = lua::L_newstate();
    if (state == nullptr) return 1;

    const int status = lua::L_dostring(state, "return 6 * 7");
    const auto answer = lua::tointeger(state, -1);
    lua::close(state);
    return status == lua::OK && answer == 42 ? 0 : 1;
}
