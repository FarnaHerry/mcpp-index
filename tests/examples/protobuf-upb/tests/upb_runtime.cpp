// Behavioral test for compat.protobuf's `upb` feature.
//
// Drives BOTH runtimes across the same bytes, which is the strongest thing
// this member can assert: the C++ runtime builds a FileDescriptorProto and
// serializes it, then upb parses those bytes and loads them into a upb_DefPool
// and answers reflection queries about them.
//
// What that reaches:
//   upb_Arena / upb_DefPool        -> upb/mem/arena.c, reflection/def_pool.c
//   parse of a descriptor message  -> upb/wire/decode.c, message/*.c, mini_table/*.c
//   AddFile + name lookups         -> upb/reflection/{file,message,field}_def.c
//   the descriptor tables themselves -> upb/reflection/cmake/…/descriptor.upb_minitable.c
//
// That last one is the reason this test exists in this shape: those tables are
// NOT in upstream's libupb_srcs, they come from cmake/libupb.cmake's
// bootstrap_sources, and leaving them out fails only at LINK time. Anything
// that merely compiles upb would not notice.
//
// Returns non-zero on any mismatch.
#include <cstddef>
#include <string>

#include "google/protobuf/descriptor.pb.h"

// NOT wrapped in extern "C": upb's headers manage their own linkage, and
// upb/message/accessors.h deliberately declares C++ OVERLOADS outside the
// extern "C" block. Forcing the whole tree into C linkage makes those overloads
// collide with their C counterparts ("conflicting declaration of C function").
#include "google/protobuf/descriptor.upb.h"
#include "upb/base/status.h"
#include "upb/mem/arena.h"
#include "upb/reflection/def_pool.h"
#include "upb/reflection/field_def.h"
#include "upb/reflection/file_def.h"
#include "upb/reflection/message_def.h"

namespace gpb = google::protobuf;

namespace {

// Build the schema with the C++ runtime and hand upb its wire bytes.
std::string make_descriptor_bytes() {
    gpb::FileDescriptorProto file;
    file.set_name("mcpp_upb_test.proto");
    file.set_package("mcpp.test");
    file.set_syntax("proto3");

    gpb::DescriptorProto* msg = file.add_message_type();
    msg->set_name("Person");

    gpb::FieldDescriptorProto* name = msg->add_field();
    name->set_name("name");
    name->set_number(1);
    name->set_type(gpb::FieldDescriptorProto::TYPE_STRING);
    name->set_label(gpb::FieldDescriptorProto::LABEL_OPTIONAL);

    gpb::FieldDescriptorProto* id = msg->add_field();
    id->set_name("id");
    id->set_number(2);
    id->set_type(gpb::FieldDescriptorProto::TYPE_INT32);
    id->set_label(gpb::FieldDescriptorProto::LABEL_OPTIONAL);

    return file.SerializeAsString();
}

}  // namespace

int main() {
    const std::string wire = make_descriptor_bytes();
    if (wire.empty()) return 1;

    upb_Arena* arena = upb_Arena_New();
    if (arena == nullptr) return 1;

    // upb parses what the C++ runtime wrote.
    google_protobuf_FileDescriptorProto* parsed =
        google_protobuf_FileDescriptorProto_parse(wire.data(), wire.size(), arena);
    if (parsed == nullptr) {
        upb_Arena_Free(arena);
        return 1;
    }

    upb_DefPool* pool = upb_DefPool_New();
    if (pool == nullptr) {
        upb_Arena_Free(arena);
        return 1;
    }

    upb_Status status;
    upb_Status_Clear(&status);
    const upb_FileDef* file_def = upb_DefPool_AddFile(pool, parsed, &status);

    int rc = 1;
    if (file_def != nullptr) {
        const upb_MessageDef* msg = upb_DefPool_FindMessageByName(pool, "mcpp.test.Person");
        if (msg != nullptr && upb_MessageDef_FieldCount(msg) == 2) {
            const upb_FieldDef* f_name = upb_MessageDef_FindFieldByName(msg, "name");
            const upb_FieldDef* f_id = upb_MessageDef_FindFieldByName(msg, "id");
            const bool names_ok =
                std::string(upb_FileDef_Name(file_def)) == "mcpp_upb_test.proto" &&
                std::string(upb_MessageDef_FullName(msg)) == "mcpp.test.Person";
            // A name that was never declared must NOT resolve — otherwise a
            // stub lookup returning something would pass everything above.
            const bool absent_ok =
                upb_DefPool_FindMessageByName(pool, "mcpp.test.Missing") == nullptr;
            if (f_name != nullptr && f_id != nullptr && names_ok && absent_ok &&
                upb_FieldDef_Number(f_name) == 1 && upb_FieldDef_Number(f_id) == 2) {
                rc = 0;
            }
        }
    }

    upb_DefPool_Free(pool);
    upb_Arena_Free(arena);
    return rc;
}
