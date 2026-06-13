#include "etlx/json/json.hpp"

namespace etlx::json {

const error::Category Category{"json"};

namespace {

// Mutable parser state for one tokenisation pass (a hand-rolled jsmn).
struct Parser {
    Span<Token>      tokens;
    size_t           pos     = 0;
    int32_t          toknext = 0;
    int32_t          toksuper = -1;
};

Token* Alloc(Parser& p) {
    if (static_cast<size_t>(p.toknext) >= p.tokens.size()) return nullptr;
    Token* t  = &p.tokens[p.toknext++];
    *t        = Token{};
    return t;
}

bool IsHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

Status ParsePrimitive(Parser& p, etl::string_view js) {
    const size_t start = p.pos;
    for (; p.pos < js.size(); ++p.pos) {
        const char c = js[p.pos];
        if (c == '\t' || c == '\r' || c == '\n' || c == ' ' ||
            c == ',' || c == ']' || c == '}' || c == ':') {
            Token* t = Alloc(p);
            if (t == nullptr) { p.pos = start; return Fail(Category, NoMemory, "tokens exhausted"); }
            t->type   = Type::Primitive;
            t->start  = static_cast<int32_t>(start);
            t->end    = static_cast<int32_t>(p.pos);
            t->parent = p.toksuper;
            --p.pos;  // re-examine the terminator in the main loop
            return Ok();
        }
        if (static_cast<unsigned char>(c) < 32) {
            p.pos = start;
            return Fail(Category, Invalid, "control char in primitive");
        }
    }
    p.pos = start;
    return Fail(Category, Partial, "unterminated primitive");
}

Status ParseString(Parser& p, etl::string_view js) {
    const size_t start = p.pos;
    ++p.pos;  // skip opening quote
    for (; p.pos < js.size(); ++p.pos) {
        const char c = js[p.pos];
        if (c == '"') {
            Token* t = Alloc(p);
            if (t == nullptr) { p.pos = start; return Fail(Category, NoMemory, "tokens exhausted"); }
            t->type   = Type::String;
            t->start  = static_cast<int32_t>(start + 1);  // inside the quotes
            t->end    = static_cast<int32_t>(p.pos);
            t->parent = p.toksuper;
            return Ok();
        }
        if (c == '\\' && p.pos + 1 < js.size()) {
            ++p.pos;
            switch (js[p.pos]) {
                case '"': case '/': case '\\': case 'b':
                case 'f': case 'r': case 'n': case 't':
                    break;
                case 'u':
                    for (int i = 0; i < 4; ++i) {
                        ++p.pos;
                        if (p.pos >= js.size()) { p.pos = start; return Fail(Category, Partial, "short \\u escape"); }
                        if (!IsHex(js[p.pos])) { p.pos = start; return Fail(Category, Invalid, "bad \\u escape"); }
                    }
                    break;
                default:
                    p.pos = start;
                    return Fail(Category, Invalid, "bad escape");
            }
        }
    }
    p.pos = start;
    return Fail(Category, Partial, "unterminated string");
}

} // namespace

Result<size_t> Parse(etl::string_view js, Span<Token> tokens) {
    Parser p{tokens};

    for (; p.pos < js.size(); ++p.pos) {
        const char c = js[p.pos];
        switch (c) {
            case '{':
            case '[': {
                Token* t = Alloc(p);
                if (t == nullptr) return Fail(Category, NoMemory, "tokens exhausted");
                if (p.toksuper != -1) {
                    tokens[p.toksuper].size++;
                    t->parent = p.toksuper;
                }
                t->type  = (c == '{') ? Type::Object : Type::Array;
                t->start = static_cast<int32_t>(p.pos);
                p.toksuper = p.toknext - 1;
                break;
            }
            case '}':
            case ']': {
                const Type type = (c == '}') ? Type::Object : Type::Array;
                int32_t i = p.toknext - 1;
                for (; i >= 0; --i) {
                    if (tokens[i].start != -1 && tokens[i].end == -1) {
                        if (tokens[i].type != type) {
                            return Fail(Category, Invalid, "mismatched bracket");
                        }
                        tokens[i].end = static_cast<int32_t>(p.pos + 1);
                        p.toksuper    = tokens[i].parent;
                        break;
                    }
                }
                if (i == -1) return Fail(Category, Invalid, "stray closing bracket");
                break;
            }
            case '"': {
                if (auto s = ParseString(p, js); !s) return Unexpected<>{s.error()};
                if (p.toksuper != -1) tokens[p.toksuper].size++;
                break;
            }
            case '\t': case '\r': case '\n': case ' ':
                break;
            case ':':
                p.toksuper = p.toknext - 1;  // next value belongs to this key
                break;
            case ',':
                if (p.toksuper != -1 &&
                    tokens[p.toksuper].type != Type::Array &&
                    tokens[p.toksuper].type != Type::Object) {
                    p.toksuper = tokens[p.toksuper].parent;  // back up to container
                }
                break;
            default: {
                if (auto s = ParsePrimitive(p, js); !s) return Unexpected<>{s.error()};
                if (p.toksuper != -1) tokens[p.toksuper].size++;
                break;
            }
        }
    }

    // Any token still open means the input was truncated.
    for (int32_t i = p.toknext - 1; i >= 0; --i) {
        if (tokens[i].start != -1 && tokens[i].end == -1) {
            return Fail(Category, Partial, "unclosed container");
        }
    }
    return static_cast<size_t>(p.toknext);
}

Result<Json> ParseToView(etl::string_view js, Span<Token> tokens) {
    auto n = Parse(js, tokens);
    if (!n) return Unexpected<>{n.error()};
    if (n.value() == 0) return Fail(Category, Invalid, "empty document");
    return Json{js, Span<const Token>{tokens.data(), n.value()}, 0};
}

etl::string_view Json::raw() const {
    if (!valid()) return {};
    const Token& t = tokens_[index_];
    if (t.start < 0 || t.end < 0 || t.end < t.start) return {};
    return src_.substr(static_cast<size_t>(t.start),
                       static_cast<size_t>(t.end - t.start));
}

bool Json::is_null() const {
    return type() == Type::Primitive && raw() == "null";
}

size_t Json::size() const {
    if (!valid()) return 0;
    const Type t = tokens_[index_].type;
    if (t == Type::Object || t == Type::Array) {
        return static_cast<size_t>(tokens_[index_].size);
    }
    return 0;
}

Json Json::operator[](etl::string_view key) const {
    if (!is_object()) return Json{};
    // Direct children of an object token are its key strings (parent == index);
    // each key's value is the immediately following token.
    for (int32_t i = index_ + 1; i < static_cast<int32_t>(tokens_.size()); ++i) {
        if (tokens_[i].parent == index_ && tokens_[i].type == Type::String) {
            Json k{src_, tokens_, i};
            if (k.raw() == key) return Json{src_, tokens_, i + 1};
        }
    }
    return Json{};
}

Json Json::operator[](size_t i) const {
    if (!is_array()) return Json{};
    size_t seen = 0;
    for (int32_t j = index_ + 1; j < static_cast<int32_t>(tokens_.size()); ++j) {
        if (tokens_[j].parent == index_) {
            if (seen == i) return Json{src_, tokens_, j};
            ++seen;
        }
    }
    return Json{};
}

Result<int64_t> Json::AsInt() const {
    if (type() != Type::Primitive) return Fail(Category, WrongType, "not a primitive");
    etl::string_view s = raw();
    if (s.empty()) return Fail(Category, Invalid, "empty number");

    size_t i   = 0;
    bool   neg = false;
    if (s[0] == '-') { neg = true; i = 1; }
    if (i >= s.size()) return Fail(Category, Invalid, "bad number");

    int64_t v = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c < '0' || c > '9') return Fail(Category, WrongType, "not an integer");
        v = v * 10 + (c - '0');
    }
    return neg ? -v : v;
}

Result<bool> Json::AsBool() const {
    if (type() != Type::Primitive) return Fail(Category, WrongType, "not a primitive");
    etl::string_view s = raw();
    if (s == "true") return true;
    if (s == "false") return false;
    return Fail(Category, WrongType, "not a boolean");
}

} // namespace etlx::json
