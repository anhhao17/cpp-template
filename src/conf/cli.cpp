#include "etlx/conf/cli.hpp"

#include <cstdint>

namespace etlx::conf {

const error::Category Category{"conf"};

namespace {

// Writes a string view to a byte writer, ignoring write errors on best-effort
// help output but propagating them to the caller.
Status Put(io::Writer& out, etl::string_view s) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(s.data());
    return out.Write(ConstByteSpan{bytes, s.size()});
}

// Pads `s` with spaces up to `width` columns (used to align help columns).
Status PutPadded(io::Writer& out, etl::string_view s, size_t width) {
    if (auto st = Put(out, s); !st) return st;
    for (size_t i = s.size(); i < width; ++i) {
        if (auto st = Put(out, " "); !st) return st;
    }
    return Ok();
}

bool StartsWithDashDash(etl::string_view s) {
    return s.size() >= 2 && s[0] == '-' && s[1] == '-';
}

} // namespace

const CliOption* CmdlineOptionsIterator::FindOption(etl::string_view name) const {
    for (const auto& opt : command_.options) {
        if (opt.long_option == name) return &opt;
    }
    return nullptr;
}

Result<Optional<OptionValue>> CmdlineOptionsIterator::Next() {
    if (index_ >= argc_) return Optional<OptionValue>{};  // done

    etl::string_view tok{argv_[index_]};
    ++index_;

    // "--" terminates option parsing; everything after is a positional arg.
    if (!args_only_ && tok == "--") {
        args_only_ = true;
        if (index_ >= argc_) return Optional<OptionValue>{};
        tok = etl::string_view{argv_[index_]};
        ++index_;
    }

    // Positional argument.
    if (args_only_ || !StartsWithDashDash(tok)) {
        OptionValue ov;
        ov.value       = tok;
        ov.is_argument = true;
        return Optional<OptionValue>{ov};
    }

    // Option, possibly in "--name=value" form.
    etl::string_view name  = tok;
    etl::string_view value;
    bool             inline_value = false;
    if (const size_t eq = tok.find('='); eq != etl::string_view::npos) {
        name         = tok.substr(0, eq);
        value        = tok.substr(eq + 1);
        inline_value = true;
    }

    const CliOption* opt = FindOption(name);
    if (opt == nullptr) {
        return Fail(Category, UnknownOption, name);
    }

    OptionValue ov;
    ov.option = opt->long_option;

    if (opt->parameter.empty()) {
        // Boolean flag: must not carry a value.
        if (inline_value) {
            return Fail(Category, UnexpectedValue, name);
        }
        return Optional<OptionValue>{ov};
    }

    // Option takes a value: either inline (--name=value) or the next token.
    if (inline_value) {
        ov.value = value;
    } else {
        if (index_ >= argc_) {
            return Fail(Category, MissingValue, name);
        }
        ov.value = etl::string_view{argv_[index_]};
        ++index_;
    }
    return Optional<OptionValue>{ov};
}

const CliCommand* FindCommand(const CliApp& app, etl::string_view name) {
    for (const auto& cmd : app.commands) {
        if (cmd.name == name) return &cmd;
    }
    return nullptr;
}

Status PrintCliHelp(const CliApp& app, io::Writer& out) {
    if (auto s = Put(out, app.name); !s) return s;
    if (!app.short_description.empty()) {
        if (auto s = Put(out, " - "); !s) return s;
        if (auto s = Put(out, app.short_description); !s) return s;
    }
    if (auto s = Put(out, "\n\n"); !s) return s;

    if (!app.long_description.empty()) {
        if (auto s = Put(out, app.long_description); !s) return s;
        if (auto s = Put(out, "\n\n"); !s) return s;
    }

    if (auto s = Put(out, "Usage:\n  "); !s) return s;
    if (auto s = Put(out, app.name); !s) return s;
    if (auto s = Put(out, " <command> [options] [arguments]\n\nCommands:\n"); !s) return s;

    for (const auto& cmd : app.commands) {
        if (auto s = Put(out, "  "); !s) return s;
        if (auto s = PutPadded(out, cmd.name, 16); !s) return s;
        if (auto s = Put(out, cmd.description); !s) return s;
        if (auto s = Put(out, "\n"); !s) return s;
    }
    return Ok();
}

Status PrintCliCommandHelp(const CliApp& app, etl::string_view command,
                           io::Writer& out) {
    const CliCommand* cmd = FindCommand(app, command);
    if (cmd == nullptr) {
        return Fail(Category, UnknownCommand, command);
    }

    if (auto s = Put(out, "Usage:\n  "); !s) return s;
    if (auto s = Put(out, app.name); !s) return s;
    if (auto s = Put(out, " "); !s) return s;
    if (auto s = Put(out, cmd->name); !s) return s;
    if (!cmd->options.empty()) {
        if (auto s = Put(out, " [options]"); !s) return s;
    }
    if (cmd->argument.has_value()) {
        if (auto s = Put(out, cmd->argument->mandatory ? " <" : " ["); !s) return s;
        if (auto s = Put(out, cmd->argument->name); !s) return s;
        if (auto s = Put(out, cmd->argument->mandatory ? ">" : "]"); !s) return s;
    }
    if (auto s = Put(out, "\n\n"); !s) return s;

    if (!cmd->description.empty()) {
        if (auto s = Put(out, cmd->description); !s) return s;
        if (auto s = Put(out, "\n"); !s) return s;
    }

    if (!cmd->options.empty()) {
        if (auto s = Put(out, "\nOptions:\n"); !s) return s;
        for (const auto& opt : cmd->options) {
            if (auto s = Put(out, "  "); !s) return s;
            // Build the "--name VALUE" left column.
            if (auto s = Put(out, opt.long_option); !s) return s;
            if (!opt.parameter.empty()) {
                if (auto s = Put(out, " "); !s) return s;
                if (auto s = Put(out, opt.parameter); !s) return s;
            }
            if (auto s = Put(out, "\n      "); !s) return s;
            if (auto s = Put(out, opt.description); !s) return s;
            if (auto s = Put(out, "\n"); !s) return s;
        }
    }
    return Ok();
}

} // namespace etlx::conf
