#include "program.hpp"
#include <glibmm.h>
#include <iostream>
#include <map>
#include <sstream>
#include <stdint.h>

namespace horizon {
ParameterProgram::ParameterProgram(const std::string &s) : code(s)
{
    if (code.size())
        init_error = compile();
}

ParameterProgram::ParameterProgram(const ParameterProgram &other) : code(other.code)
{
    std::transform(other.tokens.begin(), other.tokens.end(), std::back_inserter(tokens),
                   [](auto &x) { return x->clone(); });
}

ParameterProgram &ParameterProgram::operator=(const ParameterProgram &other)
{
    code = other.code;
    tokens.clear();
    std::transform(other.tokens.begin(), other.tokens.end(), std::back_inserter(tokens),
                   [](auto &x) { return x->clone(); });
    return *this;
}

std::optional<std::string> ParameterProgram::get_init_error()
{
    return init_error;
}

const std::string &ParameterProgram::get_code() const
{
    return code;
}

std::optional<std::string> ParameterProgram::set_code(const std::string &s)
{
    code = s;
    return compile();
}

bool ParameterProgram::stack_pop(int64_t &va)
{
    if (stack.size()) {
        va = stack.back();
        stack.pop_back();
        return false;
    }
    else {
        return true;
    }
}


std::optional<std::string> ParameterProgram::cmd_dump(const TokenCommand &cmd)
{
    auto sz = stack.size();
    for (const auto &it : stack) {
        sz--;
        std::cout << sz << ": " << it << "\n";
    }
    std::cout << std::endl;
    return {};
}

std::optional<std::string> ParameterProgram::cmd_math1(const TokenCommand &cmd)
{
    int64_t a;
    if (stack_pop(a))
        return "empty stack";
    if (cmd.command == "dup") {
        stack.push_back(a);
        stack.push_back(a);
    }
    else if (cmd.command == "chs") {
        stack.push_back(-a);
    }
    return {};
}

std::optional<std::string> ParameterProgram::cmd_math3(const TokenCommand &cmd)
{
    int64_t a, b, c;
    if (stack_pop(c) || stack_pop(b) || stack_pop(a))
        return "empty stack";
    if (cmd.command == "+xy") {
        stack.push_back(a + c);
        stack.push_back(b + c);
    }
    else if (cmd.command == "-xy") {
        stack.push_back(a - c);
        stack.push_back(b - c);
    }
    return {};
}

std::optional<std::string> ParameterProgram::cmd_math2(const TokenCommand &cmd)
{
    int64_t a, b;
    if (stack_pop(b) || stack_pop(a))
        return "empty stack";
    if (cmd.command[0] == '+') {
        stack.push_back(a + b);
    }
    else if (cmd.command[0] == '-') {
        stack.push_back(a - b);
    }
    else if (cmd.command[0] == '*') {
        stack.push_back(a * b);
    }
    else if (cmd.command[0] == '/') {
        stack.push_back(a / b);
    }
    else if (cmd.command == "dupc") {
        stack.push_back(a);
        stack.push_back(b);
        stack.push_back(a);
        stack.push_back(b);
    }
    else if (cmd.command == "swap") {
        stack.push_back(b);
        stack.push_back(a);
    }
    return {};
}

ParameterProgram::CommandHandler ParameterProgram::get_command(const std::string &cmd)
{
    using namespace std::placeholders;
    static const std::map<std::string, ParameterProgram::CommandHandler> commands = {
            {"dump", &ParameterProgram::cmd_dump},  {"+", &ParameterProgram::cmd_math2},
            {"-", &ParameterProgram::cmd_math2},    {"*", &ParameterProgram::cmd_math2},
            {"/", &ParameterProgram::cmd_math2},    {"dup", &ParameterProgram::cmd_math1},
            {"dupc", &ParameterProgram::cmd_math2}, {"swap", &ParameterProgram::cmd_math2},
            {"+xy", &ParameterProgram::cmd_math3},  {"-xy", &ParameterProgram::cmd_math3},
            {"chs", &ParameterProgram::cmd_math1},
    };

    if (commands.count(cmd))
        return commands.at(cmd);
    else
        return nullptr;
}

std::optional<std::string> ParameterProgram::run(const ParameterSet &pset)
{
    stack.clear();
    for (const auto &token : tokens) {
        switch (token->type) {
        case Token::Type::CMD: {
            auto &tok = dynamic_cast<const TokenCommand &>(*token.get());
            if (auto cmd = get_command(tok.command)) {
                auto r = std::invoke(cmd, *this, tok);
                if (r.has_value()) {
                    return r;
                }
            }
            else if (tok.command == "get-parameter") {
                if (tok.arguments.size() < 1 || tok.arguments.at(0)->type != Token::Type::STR) {
                    return "get-parameter requires one string argument";
                }
                auto &arg = dynamic_cast<const TokenString &>(*tok.arguments.at(0).get()).string;
                ParameterID pid = parameter_id_from_string(arg);
                if (pid == ParameterID::INVALID) {
                    return "invalid parameter " + arg;
                }
                if (pset.count(pid) == 0) {
                    return "parameter not found: " + parameter_id_to_string(pid);
                }
                stack.push_back(pset.at(pid));
            }
            else {
                return "unknown command " + tok.command;
            }
        } break;
        case Token::Type::INT: {
            auto &tok = dynamic_cast<const TokenInt &>(*token.get());
            stack.push_back(tok.value);
        } break;
        case Token::Type::STR:
        case Token::Type::UUID:
            break;
        }
    }

    return {};
}


std::optional<std::string> ParameterProgram::compile()
{
    std::stringstream iss(code);
    std::vector<std::string> stokens{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

    const auto regex_int = Glib::Regex::create("^([+-]?\\d+)$");
    const auto regex_dim = Glib::Regex::create("^([+-]?(?:\\d*\\.)?\\d+)mm$");
    const auto regex_str = Glib::Regex::create("^([a-z][a-z-_0-9]*)$");
    const auto regex_math = Glib::Regex::create("^([+-/\x2A][a-z]*)$");
    const auto regex_uuid = Glib::Regex::create(
            "^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-"
            "f]{12}$");
    tokens.clear();

    bool arg_mode = false;
    for (const auto &it : stokens) {
        Glib::ustring token(it);
        Glib::MatchInfo ma;
        // std::cout << "tok " << it << std::endl;
        auto &ts = arg_mode ? dynamic_cast<TokenCommand &>(*tokens.back().get()).arguments : tokens;
        if (regex_math->match(token, ma)) {
            tokens.push_back(std::make_unique<TokenCommand>(ma.fetch(1)));
        }
        else if (regex_int->match(token, ma)) {
            ts.push_back(std::make_unique<TokenInt>(std::stoi(ma.fetch(1))));
        }
        else if (regex_dim->match(token, ma)) {
            double f;
            std::istringstream istr(ma.fetch(1));
            istr.imbue(std::locale::classic());
            istr >> f;
            ts.push_back(std::make_unique<TokenInt>(1e6 * f));
        }
        else if (regex_str->match(token, ma) && !arg_mode) {
            tokens.push_back(std::make_unique<TokenCommand>(ma.fetch(1)));
        }
        else if (regex_uuid->match(token, ma) && arg_mode) {
            ts.push_back(std::make_unique<TokenUUID>(ma.fetch(1)));
        }
        else if (regex_str->match(token, ma) && arg_mode) {
            ts.push_back(std::make_unique<TokenString>(ma.fetch(1)));
        }
        else if (token == "[") {
            if (arg_mode == true) {
                return "repeated [";
            }
            if (tokens.back()->type != Token::Type::CMD) {
                return "[ has to follow command token";
            }
            arg_mode = true;
        }
        else if (token == "]") {
            if (arg_mode == false) {
                return "repeated ]";
            }
            arg_mode = false;
        }
        else {
            return "unhandled token " + token;
        }
    }
    return {};
}
} // namespace horizon
