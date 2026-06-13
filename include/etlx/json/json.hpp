#ifndef ETLX_JSON_JSON_HPP
#define ETLX_JSON_JSON_HPP

#include "etlx/core/result.hpp"
#include "etlx/core/span.hpp"

#include <cstdint>

#include <etl/string_view.h>

// Bounded, heap-free JSON reader. Parsing tokenises the input into a
// caller-supplied fixed array of Tokens (a jsmn-style flat document), then a
// lightweight Json view navigates that array without copying the source. Suits
// config blobs, manifests and small HTTP bodies on constrained targets.
namespace etlx::json {

extern const error::Category Category;

enum Code : error::Code {
    None      = 0,
    NoMemory  = 1,  // ran out of Token slots
    Invalid   = 2,  // malformed JSON
    Partial   = 3,  // input ended mid-value
    WrongType = 4,  // accessor used on a value of another type
};

enum class Type : uint8_t {
    Undefined = 0,
    Object,
    Array,
    String,
    Primitive,  // number, true, false, or null
};

// One parsed node. start/end index into the original source; size is the child
// count; parent indexes the enclosing token (-1 for the root).
struct Token {
    Type    type   = Type::Undefined;
    int32_t start  = -1;
    int32_t end    = -1;
    int32_t size   = 0;
    int32_t parent = -1;
};

// Tokenises `json` into `tokens`, returning the number of tokens used, or an
// error (NoMemory if the array is too small, Invalid/Partial if malformed).
Result<size_t> Parse(etl::string_view json, Span<Token> tokens);

// A read-only cursor over one token in a parsed document. Copyable and cheap;
// holds only references to the source and token array. An invalid view (e.g.
// from a missing member) reports Type::Undefined and yields errors on access.
class Json {
public:
    Json() = default;
    Json(etl::string_view src, Span<const Token> tokens, int32_t index)
        : src_(src), tokens_(tokens), index_(index) {}

    bool valid() const { return index_ >= 0 && index_ < static_cast<int32_t>(tokens_.size()); }
    Type type() const { return valid() ? tokens_[index_].type : Type::Undefined; }

    bool is_object() const { return type() == Type::Object; }
    bool is_array() const { return type() == Type::Array; }
    bool is_string() const { return type() == Type::String; }
    bool is_null() const;

    // Number of members (object) or elements (array); 0 otherwise.
    size_t size() const;

    // Object member by key, or an invalid Json if absent / not an object.
    Json operator[](etl::string_view key) const;

    // Array element by index, or an invalid Json if out of range / not array.
    Json operator[](size_t i) const;

    // The raw source slice for this token (without surrounding quotes for
    // strings). Empty for invalid views. Escape sequences are not decoded.
    etl::string_view raw() const;
    etl::string_view AsString() const { return raw(); }

    Result<int64_t> AsInt() const;
    Result<bool>    AsBool() const;

private:
    etl::string_view  src_;
    Span<const Token> tokens_;
    int32_t           index_ = -1;
};

// Convenience: parse then return a Json view of the root token.
Result<Json> ParseToView(etl::string_view json, Span<Token> tokens);

} // namespace etlx::json

#endif // ETLX_JSON_JSON_HPP
