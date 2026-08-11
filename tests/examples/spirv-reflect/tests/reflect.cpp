// compat.spirv-reflect — reflect a REAL compute shader and assert the bindings
// come back with the values the shader declared.
//
// The module below is the SPIR-V that glslc emits for this GLSL:
//
//     #version 450
//     layout(set = 0, binding = 3, std430) buffer Data   { float values[]; } data;
//     layout(set = 1, binding = 0)         uniform Params { vec4 tint; }  params;
//     layout(push_constant)                uniform Push   { mat4 mvp;  }  push;
//     layout(local_size_x = 64) in;
//     void main() { data.values[gl_GlobalInvocationID.x] *= params.tint.x + push.mvp[0][0]; }
//
// It is embedded as words rather than compiled at test time so the test needs
// no shader compiler on the runner. Two sets, two different descriptor types, a
// non-zero binding number and a push-constant block mean a stub that returned
// zeroed structures could not pass.
#include <spirv_reflect.h>
import std;

namespace {
// glslc -fshader-stage=compute, SPIR-V 1.0 / Vulkan 1.0
constexpr std::uint32_t kComputeSpv[] = {
    0x07230203, 0x00010000, 0x000d000b, 0x0000002b, 0x00000000, 0x00020011,
    0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0006000f, 0x00000005,
    0x00000004, 0x6e69616d, 0x00000000, 0x00000010, 0x00060010, 0x00000004,
    0x00000011, 0x00000040, 0x00000001, 0x00000001, 0x00030003, 0x00000002,
    0x000001c2, 0x000a0004, 0x475f4c47, 0x4c474f4f, 0x70635f45, 0x74735f70,
    0x5f656c79, 0x656e696c, 0x7269645f, 0x69746365, 0x00006576, 0x00080004,
    0x475f4c47, 0x4c474f4f, 0x6e695f45, 0x64756c63, 0x69645f65, 0x74636572,
    0x00657669, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00040005,
    0x00000008, 0x61746144, 0x00000000, 0x00050006, 0x00000008, 0x00000000,
    0x756c6176, 0x00007365, 0x00040005, 0x0000000a, 0x61746164, 0x00000000,
    0x00080005, 0x00000010, 0x475f6c67, 0x61626f6c, 0x766e496c, 0x7461636f,
    0x496e6f69, 0x00000044, 0x00040005, 0x00000016, 0x61726150, 0x0000736d,
    0x00050006, 0x00000016, 0x00000000, 0x746e6974, 0x00000000, 0x00040005,
    0x00000018, 0x61726170, 0x0000736d, 0x00040005, 0x0000001d, 0x68737550,
    0x00000000, 0x00040006, 0x0000001d, 0x00000000, 0x0070766d, 0x00040005,
    0x0000001f, 0x68737570, 0x00000000, 0x00040047, 0x00000007, 0x00000006,
    0x00000004, 0x00050048, 0x00000008, 0x00000000, 0x00000023, 0x00000000,
    0x00030047, 0x00000008, 0x00000003, 0x00040047, 0x0000000a, 0x00000022,
    0x00000000, 0x00040047, 0x0000000a, 0x00000021, 0x00000003, 0x00040047,
    0x00000010, 0x0000000b, 0x0000001c, 0x00050048, 0x00000016, 0x00000000,
    0x00000023, 0x00000000, 0x00030047, 0x00000016, 0x00000002, 0x00040047,
    0x00000018, 0x00000022, 0x00000001, 0x00040047, 0x00000018, 0x00000021,
    0x00000000, 0x00040048, 0x0000001d, 0x00000000, 0x00000005, 0x00050048,
    0x0000001d, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x0000001d,
    0x00000000, 0x00000007, 0x00000010, 0x00030047, 0x0000001d, 0x00000002,
    0x00040047, 0x0000002a, 0x0000000b, 0x00000019, 0x00020013, 0x00000002,
    0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020,
    0x0003001d, 0x00000007, 0x00000006, 0x0003001e, 0x00000008, 0x00000007,
    0x00040020, 0x00000009, 0x00000002, 0x00000008, 0x0004003b, 0x00000009,
    0x0000000a, 0x00000002, 0x00040015, 0x0000000b, 0x00000020, 0x00000001,
    0x0004002b, 0x0000000b, 0x0000000c, 0x00000000, 0x00040015, 0x0000000d,
    0x00000020, 0x00000000, 0x00040017, 0x0000000e, 0x0000000d, 0x00000003,
    0x00040020, 0x0000000f, 0x00000001, 0x0000000e, 0x0004003b, 0x0000000f,
    0x00000010, 0x00000001, 0x0004002b, 0x0000000d, 0x00000011, 0x00000000,
    0x00040020, 0x00000012, 0x00000001, 0x0000000d, 0x00040017, 0x00000015,
    0x00000006, 0x00000004, 0x0003001e, 0x00000016, 0x00000015, 0x00040020,
    0x00000017, 0x00000002, 0x00000016, 0x0004003b, 0x00000017, 0x00000018,
    0x00000002, 0x00040020, 0x00000019, 0x00000002, 0x00000006, 0x00040018,
    0x0000001c, 0x00000015, 0x00000004, 0x0003001e, 0x0000001d, 0x0000001c,
    0x00040020, 0x0000001e, 0x00000009, 0x0000001d, 0x0004003b, 0x0000001e,
    0x0000001f, 0x00000009, 0x00040020, 0x00000020, 0x00000009, 0x00000006,
    0x0004002b, 0x0000000d, 0x00000028, 0x00000040, 0x0004002b, 0x0000000d,
    0x00000029, 0x00000001, 0x0006002c, 0x0000000e, 0x0000002a, 0x00000028,
    0x00000029, 0x00000029, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
    0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x00000012, 0x00000013,
    0x00000010, 0x00000011, 0x0004003d, 0x0000000d, 0x00000014, 0x00000013,
    0x00060041, 0x00000019, 0x0000001a, 0x00000018, 0x0000000c, 0x00000011,
    0x0004003d, 0x00000006, 0x0000001b, 0x0000001a, 0x00070041, 0x00000020,
    0x00000021, 0x0000001f, 0x0000000c, 0x0000000c, 0x00000011, 0x0004003d,
    0x00000006, 0x00000022, 0x00000021, 0x00050081, 0x00000006, 0x00000023,
    0x0000001b, 0x00000022, 0x00060041, 0x00000019, 0x00000024, 0x0000000a,
    0x0000000c, 0x00000014, 0x0004003d, 0x00000006, 0x00000025, 0x00000024,
    0x00050085, 0x00000006, 0x00000026, 0x00000025, 0x00000023, 0x00060041,
    0x00000019, 0x00000027, 0x0000000a, 0x0000000c, 0x00000014, 0x0003003e,
    0x00000027, 0x00000026, 0x000100fd, 0x00010038,};
}  // namespace

int main() {
    bool ok = true;
    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    SpvReflectShaderModule mod{};
    const auto rc = spvReflectCreateShaderModule(sizeof(kComputeSpv), kComputeSpv, &mod);
    check(rc == SPV_REFLECT_RESULT_SUCCESS, "module parsed");
    if (rc != SPV_REFLECT_RESULT_SUCCESS) return 1;

    check(mod.shader_stage == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT, "stage is compute");
    check(std::string_view{mod.entry_point_name} == "main", "entry point is main");

    // ---- descriptor bindings --------------------------------------------
    std::uint32_t count = 0;
    check(spvReflectEnumerateDescriptorBindings(&mod, &count, nullptr) == SPV_REFLECT_RESULT_SUCCESS,
          "binding count query");
    check(count == 2, "two descriptor bindings");

    std::vector<SpvReflectDescriptorBinding*> bindings(count);
    check(spvReflectEnumerateDescriptorBindings(&mod, &count, bindings.data()) == SPV_REFLECT_RESULT_SUCCESS,
          "binding enumeration");

    bool saw_storage = false, saw_uniform = false;
    for (const auto* b : bindings) {
        if (b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            saw_storage = true;
            check(b->set == 0,     "storage buffer is in set 0");
            check(b->binding == 3, "storage buffer is at binding 3");
        } else if (b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            saw_uniform = true;
            check(b->set == 1,     "uniform buffer is in set 1");
            check(b->binding == 0, "uniform buffer is at binding 0");
        }
    }
    check(saw_storage, "the std430 buffer was reflected as a storage buffer");
    check(saw_uniform, "the uniform block was reflected as a uniform buffer");

    // ---- descriptor sets -------------------------------------------------
    std::uint32_t sets = 0;
    check(spvReflectEnumerateDescriptorSets(&mod, &sets, nullptr) == SPV_REFLECT_RESULT_SUCCESS,
          "set count query");
    check(sets == 2, "two descriptor sets");

    // ---- push constants --------------------------------------------------
    std::uint32_t blocks = 0;
    check(spvReflectEnumeratePushConstantBlocks(&mod, &blocks, nullptr) == SPV_REFLECT_RESULT_SUCCESS,
          "push constant count query");
    check(blocks == 1, "one push constant block");

    std::vector<SpvReflectBlockVariable*> pcs(blocks);
    if (blocks == 1) {
        check(spvReflectEnumeratePushConstantBlocks(&mod, &blocks, pcs.data()) == SPV_REFLECT_RESULT_SUCCESS,
              "push constant enumeration");
        check(pcs[0]->size == 64, "the mat4 push constant block is 64 bytes");
        check(pcs[0]->offset == 0, "push constant block starts at offset 0");
    }

    spvReflectDestroyShaderModule(&mod);

    // ---- negative case ---------------------------------------------------
    // Garbage must be REJECTED. Without this, a stub that always returns
    // SUCCESS would pass everything above by accident.
    const std::uint32_t junk[] = {0xdeadbeefu, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    SpvReflectShaderModule bad{};
    check(spvReflectCreateShaderModule(sizeof(junk), junk, &bad) != SPV_REFLECT_RESULT_SUCCESS,
          "a bad magic number is rejected");

    if (ok) std::println("SPIRV-Reflect OK");
    return ok ? 0 : 1;
}
