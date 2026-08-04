// Behavioral test for compat.protobuf (the libprotobuf runtime).
//
// Deliberately uses NO protoc-generated code: the package ships the runtime,
// not the compiler, so this drives the surface a consumer actually gets —
//
//   .proto text -> descriptor      compiler/parser.cc, io/tokenizer.cc
//   descriptor -> message          descriptor.cc, dynamic_message.cc
//   set/get fields                 generated_message_reflection.cc, map_field.cc
//   serialize / parse              wire_format.cc, parse_context.cc, coded_stream.cc
//   TextFormat                     text_format.cc
//   JSON                           json/json.cc + json/internal/*.cc
//   well-known types               timestamp.pb.cc, struct.pb.cc, util/time_util.cc
//   MessageDifferencer             util/message_differencer.cc
//   UTF-8 validation               third_party/utf8_range/utf8_range.c
//
// Returns non-zero on any mismatch.
#include <memory>
#include <string>

#include "google/protobuf/compiler/parser.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/json/json.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "google/protobuf/util/time_util.h"

namespace gpb = google::protobuf;

namespace {

// Collects parse errors instead of printing them, so a broken schema shows up
// as a failed assertion rather than noise on stderr.
class SilentErrorCollector : public gpb::io::ErrorCollector {
public:
    void RecordError(int line, gpb::io::ColumnNumber col, absl::string_view msg) override {
        ++errors;
        (void)line;
        (void)col;
        (void)msg;
    }
    int errors = 0;
};

constexpr const char* kSchema = R"(
    syntax = "proto3";
    package mcpp.test;
    message Person {
      string name = 1;
      int32  id = 2;
      repeated string tags = 3;
      map<string, int32> scores = 4;
    }
)";

// Parse the .proto TEXT above into a FileDescriptorProto. This is the runtime's
// own parser (compiler/parser.cc), not protoc.
bool build_pool(gpb::DescriptorPool& pool, const gpb::Descriptor** out) {
    gpb::io::ArrayInputStream raw(kSchema, static_cast<int>(std::string(kSchema).size()));
    SilentErrorCollector collector;
    gpb::io::Tokenizer tokenizer(&raw, &collector);

    gpb::FileDescriptorProto file;
    gpb::compiler::Parser parser;
    parser.RecordErrorsTo(&collector);
    if (!parser.Parse(&tokenizer, &file) || collector.errors != 0) return false;
    file.set_name("mcpp_test.proto");

    const gpb::FileDescriptor* fd = pool.BuildFile(file);
    if (fd == nullptr) return false;
    *out = pool.FindMessageTypeByName("mcpp.test.Person");
    return *out != nullptr;
}

bool dynamic_message_roundtrip(const gpb::Descriptor* desc) {
    gpb::DynamicMessageFactory factory;
    const gpb::Message* prototype = factory.GetPrototype(desc);
    if (prototype == nullptr) return false;

    std::unique_ptr<gpb::Message> msg(prototype->New());
    const gpb::Reflection* ref = msg->GetReflection();

    ref->SetString(msg.get(), desc->FindFieldByName("name"), "Ada");
    ref->SetInt32(msg.get(), desc->FindFieldByName("id"), 1815);
    const gpb::FieldDescriptor* tags = desc->FindFieldByName("tags");
    ref->AddString(msg.get(), tags, "math");
    ref->AddString(msg.get(), tags, "engine");

    // Wire round-trip.
    std::string wire;
    if (!msg->SerializeToString(&wire)) return false;
    std::unique_ptr<gpb::Message> back(prototype->New());
    if (!back->ParseFromString(wire)) return false;
    if (!gpb::util::MessageDifferencer::Equals(*msg, *back)) return false;

    // TextFormat round-trip.
    std::string text;
    if (!gpb::TextFormat::PrintToString(*back, &text)) return false;
    if (text.find("name: \"Ada\"") == std::string::npos) return false;
    if (text.find("id: 1815") == std::string::npos) return false;
    std::unique_ptr<gpb::Message> from_text(prototype->New());
    if (!gpb::TextFormat::ParseFromString(text, from_text.get())) return false;
    if (!gpb::util::MessageDifferencer::Equals(*msg, *from_text)) return false;

    // A truncated payload must be REJECTED — proves parsing is real and not a
    // no-op that accepts anything.
    std::unique_ptr<gpb::Message> truncated(prototype->New());
    if (!wire.empty() && truncated->ParseFromString(wire.substr(0, wire.size() - 1))) return false;

    return ref->GetRepeatedString(*back, tags, 1) == "engine";
}

bool json_ok(const gpb::Descriptor* desc) {
    gpb::DynamicMessageFactory factory;
    std::unique_ptr<gpb::Message> msg(factory.GetPrototype(desc)->New());
    const gpb::Reflection* ref = msg->GetReflection();
    ref->SetString(msg.get(), desc->FindFieldByName("name"), "Grace");
    ref->SetInt32(msg.get(), desc->FindFieldByName("id"), 1906);

    std::string json;
    if (!gpb::json::MessageToJsonString(*msg, &json).ok()) return false;
    if (json.find("\"name\":\"Grace\"") == std::string::npos) return false;

    std::unique_ptr<gpb::Message> back(factory.GetPrototype(desc)->New());
    if (!gpb::json::JsonStringToMessage(json, back.get()).ok()) return false;
    return gpb::util::MessageDifferencer::Equals(*msg, *back);
}

// The well-known types are generated code that upstream CHECKS IN, so they are
// compiled into this package and usable with no protoc anywhere.
bool well_known_types_ok() {
    gpb::Timestamp ts;
    if (!gpb::util::TimeUtil::FromString("2026-08-04T12:34:56Z", &ts)) return false;
    if (gpb::util::TimeUtil::ToString(ts) != "2026-08-04T12:34:56Z") return false;

    gpb::Struct s;
    (*s.mutable_fields())["pi"].set_number_value(3.5);
    (*s.mutable_fields())["name"].set_string_value("mcpp");
    if (s.fields().size() != 2) return false;
    if (s.fields().at("pi").number_value() != 3.5) return false;

    std::string wire;
    if (!s.SerializeToString(&wire)) return false;
    gpb::Struct back;
    return back.ParseFromString(wire) && back.fields().at("name").string_value() == "mcpp";
}

// proto3 string fields are UTF-8 validated, which routes into
// third_party/utf8_range/utf8_range.c — the one C TU in this package.
bool utf8_validation_ok(const gpb::Descriptor* desc) {
    gpb::DynamicMessageFactory factory;
    std::unique_ptr<gpb::Message> msg(factory.GetPrototype(desc)->New());
    msg->GetReflection()->SetString(msg.get(), desc->FindFieldByName("name"), "héllo-世界");

    std::string wire;
    if (!msg->SerializeToString(&wire)) return false;
    std::unique_ptr<gpb::Message> back(factory.GetPrototype(desc)->New());
    if (!back->ParseFromString(wire)) return false;
    return back->GetReflection()->GetString(*back, desc->FindFieldByName("name")) == "héllo-世界";
}

}  // namespace

int main() {
    gpb::DescriptorPool pool;
    const gpb::Descriptor* person = nullptr;
    if (!build_pool(pool, &person)) return 1;

    const bool ok = dynamic_message_roundtrip(person) && json_ok(person)
                    && well_known_types_ok() && utf8_validation_ok(person);
    return ok ? 0 : 1;
}
