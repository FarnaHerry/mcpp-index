// Consuming `mcpplibs.llmapi` through the published index.
//
// Every assertion here is OFFLINE, on purpose. This is an HTTP client, and a
// test that reaches an LLM endpoint needs a key CI does not have, costs money,
// and fails for reasons that have nothing to do with the package. What a
// workspace member is for is the dependency edge: does the module import,
// does its exported surface exist and behave, does it link.
//
// The partitions exercised (:url, :types, :errors) are pure data and pure
// logic, which is exactly why they are the right ones to assert on.
import std;
import mcpplibs.llmapi;

using namespace mcpplibs;

int main() {
    int failures = 0;
    auto check = [&](bool ok, std::string_view what) {
        if (!ok) { std::println("FAIL: {}", what); ++failures; }
    };

    // :url — the provider endpoints are compile-time constants, so a wrong one
    // is a wrong request forever. Asserting the shape catches a typo'd scheme
    // or a dropped /v1 that would only surface as a 404 at runtime.
    check(llmapi::URL::OpenAI.starts_with("https://"), "OpenAI endpoint is https");
    check(llmapi::URL::OpenAI.ends_with("/v1"),        "OpenAI endpoint is versioned");
    check(llmapi::URL::Anthropic.starts_with("https://"), "Anthropic endpoint is https");
    check(llmapi::URL::DeepSeek.ends_with("/v1"),      "DeepSeek endpoint is versioned");
    check(llmapi::URL::OpenAI != llmapi::URL::Anthropic, "providers are distinct");

    // :types — the variant-based content model. Constructing and visiting it
    // is what a consumer does before any request exists.
    llmapi::Content plain = std::string{"hello"};
    check(std::holds_alternative<std::string>(plain), "plain text content");

    std::vector<llmapi::ContentPart> parts;
    parts.emplace_back(llmapi::TextContent{"describe this"});
    parts.emplace_back(llmapi::ImageContent{"https://example.invalid/x.png",
                                            "image/png", /*isUrl=*/true});
    llmapi::Content multimodal = parts;
    check(std::holds_alternative<std::vector<llmapi::ContentPart>>(multimodal),
          "multimodal content");
    check(std::get<std::vector<llmapi::ContentPart>>(multimodal).size() == 2,
          "both parts survive");
    check(std::holds_alternative<llmapi::ImageContent>(
              std::get<std::vector<llmapi::ContentPart>>(multimodal)[1]),
          "the image part keeps its alternative");

    check(llmapi::Role::User != llmapi::Role::Assistant, "roles are distinct");

    // :errors — the type an unhappy call throws. A consumer catches these by
    // type, so the hierarchy is part of the contract.
    try {
        throw llmapi::ApiError(429, "rate_limit", "{}", "slow down");
    } catch (const std::runtime_error& e) {
        check(std::string_view(e.what()) == "slow down", "ApiError carries its message");
    }
    try {
        throw llmapi::ApiError(500, "server", "{}", "boom");
    } catch (const llmapi::ApiError& e) {
        check(e.statusCode == 500, "ApiError carries its status");
        check(e.type == "server",  "ApiError carries its type");
    }

    if (failures == 0) std::println("llmapi: ok");
    return failures == 0 ? 0 : 1;
}
