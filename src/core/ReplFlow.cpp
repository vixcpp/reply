/**
 *  @file ReplFlow.cpp
 *  @brief Main execution flow implementation for the Vix Reply REPL.
 *
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira. All rights reserved.
 *  https://github.com/vixcpp/reply
 *
 *  Use of this source code is governed by an MIT license
 *  that can be found in the LICENSE file.
 *
 *  Vix Reply
 */

#include <vix/reply/core/ReplDetail.hpp>
#include <vix/reply/core/ReplDispatcher.hpp>
#include <vix/reply/core/ReplHistory.hpp>
#include <vix/reply/core/ReplMath.hpp>
#include <vix/reply/core/ReplUtils.hpp>
#include <vix/reply/core/ReplFlow.hpp>
#include <vix/reply/core/ReplLineEditor.hpp>

#include <vix/reply/api/Vix.hpp>
#include <vix/reply/api/ReplCallParser.hpp>
#include <vix/reply/api/ReplApi.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <climits>
#include <cstddef>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

#ifndef VIX_REPLY_VERSION
#define VIX_REPLY_VERSION "dev"
#endif

#ifndef _WIN32
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
  [[maybe_unused]] constexpr unsigned char KEY_CTRL_C = 0x03;
  [[maybe_unused]] constexpr unsigned char KEY_CTRL_D = 0x04;
  [[maybe_unused]] constexpr unsigned char KEY_CTRL_L = 0x0C;
  [[maybe_unused]] constexpr unsigned char KEY_BACKSPACE_1 = 0x08;
  [[maybe_unused]] constexpr unsigned char KEY_BACKSPACE_2 = 0x7F;
  [[maybe_unused]] constexpr unsigned char KEY_ENTER_1 = '\n';
  [[maybe_unused]] constexpr unsigned char KEY_ENTER_2 = '\r';

  static bool try_eval_math_with_vars(
      const std::string &expr,
      const std::unordered_map<std::string, nlohmann::json> &vars,
      nlohmann::json &valueOut,
      std::string &formattedOut,
      std::string &err);

#ifndef _WIN32
  struct TerminalRawMode
  {
    termios old{};
    bool available = false;
    bool enabled = false;

    TerminalRawMode()
    {
      if (!isatty(STDIN_FILENO))
      {
        return;
      }

      if (tcgetattr(STDIN_FILENO, &old) != 0)
      {
        return;
      }

      available = true;
      enable();
    }

    void enable()
    {
      if (!available || enabled)
      {
        return;
      }

      termios raw = old;

      raw.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
      raw.c_lflag &= static_cast<unsigned>(~(IEXTEN));
      raw.c_lflag &= static_cast<unsigned>(~(ISIG));

      raw.c_iflag &= static_cast<unsigned>(~(IXON | ICRNL));

      raw.c_cc[VMIN] = 1;
      raw.c_cc[VTIME] = 0;

      if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
      {
        enabled = true;
      }
    }

    void disable()
    {
      if (!enabled)
      {
        return;
      }

      tcsetattr(STDIN_FILENO, TCSANOW, &old);
      enabled = false;
    }

    ~TerminalRawMode()
    {
      disable();
    }
  };
#endif

  static void print_banner()
  {
    std::cout << "Vix Reply " << VIX_REPLY_VERSION << "  REPL\n";

#if defined(__clang__)
    std::cout << "clang " << __clang_major__ << "." << __clang_minor__;
#elif defined(__GNUC__)
    std::cout << "gcc " << __GNUC__ << "." << __GNUC_MINOR__;
#else
    std::cout << "c++";
#endif

#if defined(_WIN32)
    std::cout << "  windows\n";
#elif defined(__APPLE__)
    std::cout << "  macos\n";
#else
    std::cout << "  linux\n";
#endif

    std::cout << "exit: Ctrl+D | clear: Ctrl+L | help\n\n";
  }

  static void print_commands_from_dispatcher()
  {
    std::cout << "Commands:\n";
    std::cout << "  CLI commands are disabled in standalone Reply.\n";
  }

  static void print_help()
  {
    std::cout
        << "\n"
        << "REPL commands:\n"
        << "  help                    Show this help\n"
        << "  exit                    Exit the REPL or Ctrl+C / Ctrl+D\n"
        << "  version                 Print version\n"
        << "  pwd                     Print current directory\n"
        << "  cd <dir>                Change directory\n"
        << "  clear                   Clear screen or Ctrl+L\n"
        << "  history                 Show history\n"
        << "  history clear           Clear history\n"
        << "\n"
        << "Math:\n"
        << "  <expr>                  Evaluate expression, for example 1+2*(3+4)\n"
        << "  calc <expr>             Evaluate expression explicitly\n"
        << "C++ snippets:\n"
        << "  :cpp                   Enter C++ snippet mode\n"
        << "  :run                   Run the current C++ snippet\n"
        << "  :cancel                Cancel C++ snippet mode\n"
        << "\n";

    print_commands_from_dispatcher();
    std::cout << "\n";
  }

  static std::string strip_prefix(std::string s, const std::string &prefix)
  {
    if (s.rfind(prefix, 0) == 0)
    {
      s = s.substr(prefix.size());
    }

    return vix::reply::trim_copy(s);
  }

  static std::string shell_quote(const std::string &value)
  {
#if defined(_WIN32)
    std::string out = "\"";

    for (char c : value)
    {
      if (c == '"')
      {
        out += "\\\"";
      }
      else
      {
        out.push_back(c);
      }
    }

    out += "\"";
    return out;
#else
    std::string out = "'";

    for (char c : value)
    {
      if (c == '\'')
      {
        out += "'\\''";
      }
      else
      {
        out.push_back(c);
      }
    }

    out += "'";
    return out;
#endif
  }

  static int count_cpp_braces_delta(const std::string &line)
  {
    int delta = 0;
    bool inString = false;
    bool inChar = false;
    bool escaping = false;

    for (char c : line)
    {
      if (escaping)
      {
        escaping = false;
        continue;
      }

      if (c == '\\')
      {
        escaping = true;
        continue;
      }

      if (inString)
      {
        if (c == '"')
        {
          inString = false;
        }

        continue;
      }

      if (inChar)
      {
        if (c == '\'')
        {
          inChar = false;
        }

        continue;
      }

      if (c == '"')
      {
        inString = true;
        continue;
      }

      if (c == '\'')
      {
        inChar = true;
        continue;
      }

      if (c == '{')
      {
        ++delta;
      }
      else if (c == '}')
      {
        --delta;
      }
    }

    return delta;
  }

  static bool looks_like_cpp_main(const std::string &line)
  {
    return line.find("main(") != std::string::npos ||
           line.find("main (") != std::string::npos;
  }

  static int run_cpp_snippet(const std::vector<std::string> &lines)
  {
    if (lines.empty())
    {
      std::cout << "error: no C++ code to run\n";
      return 1;
    }

    std::error_code ec;

    const fs::path root =
        fs::temp_directory_path(ec) / "vix-reply";

    if (ec)
    {
      std::cout << "error: cannot resolve temporary directory: " << ec.message() << "\n";
      return 1;
    }

    fs::create_directories(root, ec);

    if (ec)
    {
      std::cout << "error: cannot create temporary directory: " << ec.message() << "\n";
      return 1;
    }

    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();

    const fs::path file =
        root / ("snippet-" + std::to_string(now) + ".cpp");

    {
      std::ofstream out(file, std::ios::trunc);

      if (!out.is_open())
      {
        std::cout << "error: cannot write C++ snippet: " << file.string() << "\n";
        return 1;
      }

      for (const auto &line : lines)
      {
        out << line << '\n';
      }
    }

#if defined(_WIN32)
    const std::string command = "vix run " + shell_quote(file.string());
#else
    const std::string command = "vix run " + shell_quote(file.string());
#endif

    const int rc = std::system(command.c_str());

    fs::remove(file, ec);

    return rc;
  }

  static std::string to_string(const vix::reply::api::CallValue &v)
  {
    if (v.is_string())
    {
      return v.as_string();
    }

    if (v.is_bool())
    {
      return v.as_bool() ? "true" : "false";
    }

    if (v.is_int())
    {
      return std::to_string(v.as_int());
    }

    if (v.is_double())
    {
      return std::to_string(v.as_double());
    }

    return "null";
  }

  static bool contains_math_ops(const std::string &s)
  {
    for (char c : s)
    {
      if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')')
      {
        return true;
      }
    }

    return false;
  }

  static bool is_ident_start(char c)
  {
    const auto u = static_cast<unsigned char>(c);
    return std::isalpha(u) || c == '_';
  }

  static bool is_ident_char(char c)
  {
    const auto u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_';
  }

  static bool looks_like_ident(const std::string &s)
  {
    if (s.empty())
    {
      return false;
    }

    if (!is_ident_start(s.front()))
    {
      return false;
    }

    for (char c : s)
    {
      if (!is_ident_char(c))
      {
        return false;
      }
    }

    return true;
  }

  static std::string format_json_scalar(const nlohmann::json &j);

  static bool resolve_value_at_path(
      const std::unordered_map<std::string, nlohmann::json> &vars,
      const std::string &raw,
      nlohmann::json &out,
      std::string &err);

  static std::string arg_to_text(
      const vix::reply::api::CallExpr &call,
      std::size_t i,
      const std::unordered_map<std::string, nlohmann::json> &vars,
      std::string &err)
  {
    err.clear();

    if (i >= call.args.size())
    {
      return {};
    }

    if (call.args[i].is_string())
    {
      return call.args[i].as_string();
    }

    if (i < call.args_raw.size())
    {
      const std::string raw = vix::reply::trim_copy(call.args_raw[i]);

      if (looks_like_ident(raw) ||
          raw.find('.') != std::string::npos ||
          raw.find('[') != std::string::npos)
      {
        nlohmann::json resolved;

        if (resolve_value_at_path(vars, raw, resolved, err))
        {
          return format_json_scalar(resolved);
        }

        if (!err.empty())
        {
          return {};
        }
      }

      if (contains_math_ops(raw))
      {
        nlohmann::json val;
        std::string formatted;

        if (!try_eval_math_with_vars(raw, vars, val, formatted, err))
        {
          return {};
        }

        return formatted;
      }
    }

    return to_string(call.args[i]);
  }

  static std::string format_json_scalar(const nlohmann::json &j)
  {
    if (j.is_string())
    {
      return j.get<std::string>();
    }

    if (j.is_boolean())
    {
      return j.get<bool>() ? "true" : "false";
    }

    if (j.is_number_integer())
    {
      return std::to_string(j.get<long long>());
    }

    if (j.is_number_unsigned())
    {
      return std::to_string(j.get<unsigned long long>());
    }

    if (j.is_number_float())
    {
      return j.dump();
    }

    if (j.is_null())
    {
      return "null";
    }

    return j.dump();
  }

  static bool try_parse_int_text(const std::string &s, long long &out)
  {
    try
    {
      std::size_t idx = 0;
      const long long v = std::stoll(s, &idx, 10);

      if (idx != s.size())
      {
        return false;
      }

      out = v;
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  static bool try_parse_double_text(const std::string &s, double &out)
  {
    try
    {
      std::size_t idx = 0;
      const double v = std::stod(s, &idx);

      if (idx != s.size())
      {
        return false;
      }

      out = v;
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  static bool json_number_to_long_long(const nlohmann::json &j, long long &out)
  {
    if (j.is_number_integer())
    {
      out = j.get<long long>();
      return true;
    }

    if (j.is_number_unsigned())
    {
      const auto v = j.get<unsigned long long>();

      if (v > static_cast<unsigned long long>(LLONG_MAX))
      {
        return false;
      }

      out = static_cast<long long>(v);
      return true;
    }

    if (j.is_number_float())
    {
      const double d = j.get<double>();
      const double id = std::round(d);

      if (std::fabs(d - id) < 1e-12 &&
          id >= static_cast<double>(LLONG_MIN) &&
          id <= static_cast<double>(LLONG_MAX))
      {
        out = static_cast<long long>(id);
        return true;
      }
    }

    return false;
  }

  static bool json_number_to_double(const nlohmann::json &j, double &out)
  {
    if (!j.is_number())
    {
      return false;
    }

    out = j.get<double>();
    return true;
  }

  static bool resolve_value_at_path(
      const std::unordered_map<std::string, nlohmann::json> &vars,
      const std::string &raw,
      nlohmann::json &out,
      std::string &err)
  {
    err.clear();

    const std::string expr = vix::reply::trim_copy(raw);

    if (expr.empty())
    {
      err = "empty expression";
      return false;
    }

    std::size_t i = 0;

    auto parse_ident = [&](std::string &name) -> bool
    {
      name.clear();

      if (i >= expr.size() || !is_ident_start(expr[i]))
      {
        return false;
      }

      const std::size_t start = i++;

      while (i < expr.size() && is_ident_char(expr[i]))
      {
        ++i;
      }

      name = expr.substr(start, i - start);
      return true;
    };

    std::string root;

    if (!parse_ident(root))
    {
      err = "expected variable name";
      return false;
    }

    auto it = vars.find(root);

    if (it == vars.end())
    {
      err = "unknown variable: " + root;
      return false;
    }

    out = it->second;

    while (i < expr.size())
    {
      if (expr[i] == '.')
      {
        ++i;

        std::string key;

        if (!parse_ident(key))
        {
          err = "expected property name after '.'";
          return false;
        }

        if (!out.is_object())
        {
          err = "value is not an object";
          return false;
        }

        auto objIt = out.find(key);

        if (objIt == out.end())
        {
          err = "unknown property: " + key;
          return false;
        }

        out = *objIt;
        continue;
      }

      if (expr[i] == '[')
      {
        ++i;

        while (i < expr.size() && std::isspace(static_cast<unsigned char>(expr[i])))
        {
          ++i;
        }

        if (i >= expr.size())
        {
          err = "unterminated '['";
          return false;
        }

        if (expr[i] == '"' || expr[i] == '\'')
        {
          const char quote = expr[i++];
          std::string key;

          while (i < expr.size() && expr[i] != quote)
          {
            if (expr[i] == '\\' && i + 1 < expr.size())
            {
              key.push_back(expr[i + 1]);
              i += 2;
              continue;
            }

            key.push_back(expr[i++]);
          }

          if (i >= expr.size() || expr[i] != quote)
          {
            err = "unterminated string key";
            return false;
          }

          ++i;

          while (i < expr.size() && std::isspace(static_cast<unsigned char>(expr[i])))
          {
            ++i;
          }

          if (i >= expr.size() || expr[i] != ']')
          {
            err = "expected ']'";
            return false;
          }

          ++i;

          if (!out.is_object())
          {
            err = "value is not an object";
            return false;
          }

          auto objIt = out.find(key);

          if (objIt == out.end())
          {
            err = "unknown property: " + key;
            return false;
          }

          out = *objIt;
          continue;
        }

        const std::size_t start = i;

        if (i < expr.size() && (expr[i] == '+' || expr[i] == '-'))
        {
          ++i;
        }

        while (i < expr.size() && std::isdigit(static_cast<unsigned char>(expr[i])))
        {
          ++i;
        }

        const std::string idxText = expr.substr(start, i - start);

        while (i < expr.size() && std::isspace(static_cast<unsigned char>(expr[i])))
        {
          ++i;
        }

        if (i >= expr.size() || expr[i] != ']')
        {
          err = "expected ']'";
          return false;
        }

        ++i;

        long long index = 0;

        if (!try_parse_int_text(idxText, index))
        {
          err = "invalid array index";
          return false;
        }

        if (!out.is_array())
        {
          err = "value is not an array";
          return false;
        }

        if (index < 0 || static_cast<std::size_t>(index) >= out.size())
        {
          err = "array index out of range";
          return false;
        }

        out = out[static_cast<std::size_t>(index)];
        continue;
      }

      err = std::string("unexpected character in path: '") + expr[i] + "'";
      return false;
    }

    return true;
  }

  static bool resolve_arg_json(
      const vix::reply::api::CallExpr &call,
      std::size_t i,
      const std::unordered_map<std::string, nlohmann::json> &vars,
      nlohmann::json &out,
      std::string &err)
  {
    err.clear();

    if (i >= call.args.size())
    {
      err = "missing argument";
      return false;
    }

    const auto &arg = call.args[i];

    if (arg.is_string())
    {
      out = arg.as_string();
      return true;
    }

    if (arg.is_bool())
    {
      out = arg.as_bool();
      return true;
    }

    if (arg.is_int())
    {
      out = arg.as_int();
      return true;
    }

    if (arg.is_double())
    {
      out = arg.as_double();
      return true;
    }

    if (i < call.args_raw.size())
    {
      const std::string raw = vix::reply::trim_copy(call.args_raw[i]);

      if (looks_like_ident(raw) ||
          raw.find('.') != std::string::npos ||
          raw.find('[') != std::string::npos)
      {
        if (resolve_value_at_path(vars, raw, out, err))
        {
          return true;
        }
      }

      if (raw == "null")
      {
        out = nullptr;
        return true;
      }

      long long iv = 0;

      if (try_parse_int_text(raw, iv))
      {
        out = iv;
        return true;
      }

      double dv = 0.0;

      if (try_parse_double_text(raw, dv))
      {
        out = dv;
        return true;
      }
    }

    out = nullptr;
    return true;
  }

  static bool get_len_of_json(const nlohmann::json &j, long long &out)
  {
    if (j.is_string())
    {
      out = static_cast<long long>(j.get<std::string>().size());
      return true;
    }

    if (j.is_array() || j.is_object())
    {
      out = static_cast<long long>(j.size());
      return true;
    }

    return false;
  }

  static bool invoke_call(
      const vix::reply::api::CallExpr &call,
      vix::reply::api::Vix &vixObj,
      const std::unordered_map<std::string, nlohmann::json> &vars)
  {
    auto print_error = [](const std::string &msg)
    {
      vix::reply::api::println("error: " + msg);
    };

    auto join_all = [&](std::string_view sep, std::string &err) -> std::optional<std::string>
    {
      std::string out;

      for (std::size_t i = 0; i < call.args.size(); ++i)
      {
        if (i != 0)
        {
          out += sep;
        }

        auto text = arg_to_text(call, i, vars, err);

        if (!err.empty())
        {
          return std::nullopt;
        }

        out += text;
      }

      return out;
    };

    auto emit_joined = [&](auto emit, std::string_view sep = " ") -> bool
    {
      std::string err;
      auto text = join_all(sep, err);

      if (!text)
      {
        print_error(err.empty() ? "invalid arguments" : err);
        return true;
      }

      emit(*text);
      return true;
    };

    auto get_string_arg = [&](std::size_t i, std::string &out, std::string &err) -> bool
    {
      err.clear();

      nlohmann::json j;

      if (!resolve_arg_json(call, i, vars, j, err))
      {
        return false;
      }

      if (!j.is_string())
      {
        err = "expected string";
        return false;
      }

      out = j.get<std::string>();
      return true;
    };

    auto get_bool_arg = [&](std::size_t i, bool &out, std::string &err) -> bool
    {
      err.clear();

      nlohmann::json j;

      if (!resolve_arg_json(call, i, vars, j, err))
      {
        return false;
      }

      if (!j.is_boolean())
      {
        err = "expected bool";
        return false;
      }

      out = j.get<bool>();
      return true;
    };

    auto get_int_arg = [&](std::size_t i, long long &out, std::string &err) -> bool
    {
      err.clear();

      nlohmann::json j;

      if (!resolve_arg_json(call, i, vars, j, err))
      {
        return false;
      }

      if (!json_number_to_long_long(j, out))
      {
        err = "expected int";
        return false;
      }

      return true;
    };

    if (call.object.empty())
    {
      const std::string &fn = call.callee;

      if (fn == "print")
      {
        return emit_joined([](const std::string &s)
                           { vix::reply::api::print(s); });
      }

      if (fn == "println")
      {
        return emit_joined([](const std::string &s)
                           { vix::reply::api::println(s); });
      }

      if (fn == "eprint")
      {
        return emit_joined([](const std::string &s)
                           { vix::reply::api::eprint(s); });
      }

      if (fn == "eprintln")
      {
        return emit_joined([](const std::string &s)
                           { vix::reply::api::eprintln(s); });
      }

      if (fn == "cwd")
      {
        if (!call.args.empty())
        {
          print_error("cwd() takes no arguments");
          return true;
        }

        vix::reply::api::println(vixObj.cwd().string());
        return true;
      }

      if (fn == "pid")
      {
        if (!call.args.empty())
        {
          print_error("pid() takes no arguments");
          return true;
        }

        vix::reply::api::println_int(static_cast<long long>(vixObj.pid()));
        return true;
      }

      if (fn == "len")
      {
        if (call.args.size() != 1)
        {
          print_error("len(value)");
          return true;
        }

        nlohmann::json j;
        std::string err;

        if (!resolve_arg_json(call, 0, vars, j, err))
        {
          print_error(err);
          return true;
        }

        long long len = 0;

        if (!get_len_of_json(j, len))
        {
          print_error("len() expects string, array or object");
          return true;
        }

        vix::reply::api::println(std::to_string(len));
        return true;
      }

      if (fn == "double" || fn == "float")
      {
        if (call.args.size() != 1)
        {
          print_error("float(value)");
          return true;
        }

        nlohmann::json j;
        std::string err;

        if (!resolve_arg_json(call, 0, vars, j, err))
        {
          print_error(err);
          return true;
        }

        double out = 0.0;

        if (j.is_string())
        {
          if (!try_parse_double_text(j.get<std::string>(), out))
          {
            print_error("cannot convert string to float");
            return true;
          }
        }
        else if (!json_number_to_double(j, out))
        {
          print_error("float() expects numeric value or numeric string");
          return true;
        }

        vix::reply::api::println(std::to_string(out));
        return true;
      }

      if (fn == "str")
      {
        if (call.args.size() != 1)
        {
          print_error("str(value)");
          return true;
        }

        nlohmann::json j;
        std::string err;

        if (!resolve_arg_json(call, 0, vars, j, err))
        {
          print_error(err);
          return true;
        }

        vix::reply::api::println(format_json_scalar(j));
        return true;
      }

      if (fn == "int")
      {
        if (call.args.size() != 1)
        {
          print_error("int(value)");
          return true;
        }

        nlohmann::json j;
        std::string err;

        if (!resolve_arg_json(call, 0, vars, j, err))
        {
          print_error(err);
          return true;
        }

        long long out = 0;

        if (j.is_string())
        {
          if (!try_parse_int_text(j.get<std::string>(), out))
          {
            print_error("cannot convert string to int");
            return true;
          }
        }
        else if (!json_number_to_long_long(j, out))
        {
          print_error("int() expects numeric value or numeric string");
          return true;
        }

        vix::reply::api::println(std::to_string(out));
        return true;
      }

      if (fn == "type")
      {
        if (call.args.size() != 1)
        {
          print_error("type(value)");
          return true;
        }

        nlohmann::json j;
        std::string err;

        if (!resolve_arg_json(call, 0, vars, j, err))
        {
          print_error(err);
          return true;
        }

        if (j.is_string())
        {
          vix::reply::api::println("string");
        }
        else if (j.is_boolean())
        {
          vix::reply::api::println("bool");
        }
        else if (j.is_number_integer() || j.is_number_unsigned())
        {
          vix::reply::api::println("int");
        }
        else if (j.is_number_float())
        {
          vix::reply::api::println("double");
        }
        else if (j.is_array())
        {
          vix::reply::api::println("array");
        }
        else if (j.is_object())
        {
          vix::reply::api::println("object");
        }
        else if (j.is_null())
        {
          vix::reply::api::println("null");
        }
        else
        {
          vix::reply::api::println("unknown");
        }

        return true;
      }

      return false;
    }

    const std::string obj = call.object;

    if (obj != "Vix" && obj != "vix")
    {
      return false;
    }

    const std::string &member = call.member;

    if (member == "cd")
    {
      if (call.args.size() != 1)
      {
        print_error("Vix.cd(path:string)");
        return true;
      }

      std::string path;
      std::string err;

      if (!get_string_arg(0, path, err))
      {
        print_error("Vix.cd(path:string)");
        return true;
      }

      auto result = vixObj.cd(path);

      if (result.code != 0 && !result.message.empty())
      {
        print_error(result.message);
      }

      return true;
    }

    if (member == "cwd")
    {
      if (!call.args.empty())
      {
        print_error("Vix.cwd() takes no arguments");
        return true;
      }

      vix::reply::api::println(vixObj.cwd().string());
      return true;
    }

    if (member == "mkdir")
    {
      if (call.args.empty() || call.args.size() > 2)
      {
        print_error("Vix.mkdir(path:string, recursive?:bool)");
        return true;
      }

      std::string path;
      std::string err;

      if (!get_string_arg(0, path, err))
      {
        print_error("Vix.mkdir(path:string, recursive?:bool)");
        return true;
      }

      bool recursive = true;

      if (call.args.size() == 2)
      {
        if (!get_bool_arg(1, recursive, err))
        {
          print_error("recursive must be bool");
          return true;
        }
      }

      auto result = vixObj.mkdir(path, recursive);

      if (result.code != 0 && !result.message.empty())
      {
        print_error(result.message);
      }

      return true;
    }

    if (member == "env")
    {
      if (call.args.size() != 1)
      {
        print_error("Vix.env(key:string)");
        return true;
      }

      std::string key;
      std::string err;

      if (!get_string_arg(0, key, err))
      {
        print_error("Vix.env(key:string)");
        return true;
      }

      auto value = vixObj.env(key);

      if (!value)
      {
        vix::reply::api::println("null");
      }
      else
      {
        vix::reply::api::println(*value);
      }

      return true;
    }

    if (member == "pid")
    {
      if (!call.args.empty())
      {
        print_error("Vix.pid() takes no arguments");
        return true;
      }

      vix::reply::api::println_int(static_cast<long long>(vixObj.pid()));
      return true;
    }

    if (member == "exit")
    {
      if (call.args.size() > 1)
      {
        print_error("Vix.exit(code?:int)");
        return true;
      }

      int code = 0;

      if (!call.args.empty())
      {
        long long tmp = 0;
        std::string err;

        if (!get_int_arg(0, tmp, err))
        {
          print_error("Vix.exit(code?:int)");
          return true;
        }

        if (tmp < static_cast<long long>(std::numeric_limits<int>::min()) ||
            tmp > static_cast<long long>(std::numeric_limits<int>::max()))
        {
          print_error("exit code out of range");
          return true;
        }

        code = static_cast<int>(tmp);
      }

      vixObj.exit(code);
      return true;
    }

    if (member == "args")
    {
      if (!call.args.empty())
      {
        print_error(
            "Vix.args() takes no arguments.\n"
            "hint: use Vix.args() to read CLI args.");
        return true;
      }

      nlohmann::json j = nlohmann::json::array();

      for (const auto &arg : vixObj.args())
      {
        j.push_back(arg);
      }

      vix::reply::api::println(j.dump());
      return true;
    }

    if (member == "history")
    {
      if (!call.args.empty())
      {
        print_error("Vix.history() takes no arguments");
        return true;
      }

      auto result = vixObj.history();

      if (result.code != 0 && !result.message.empty())
      {
        print_error(result.message);
      }

      return true;
    }

    if (member == "history_clear")
    {
      if (!call.args.empty())
      {
        print_error("Vix.history_clear() takes no arguments");
        return true;
      }

      auto result = vixObj.history_clear();

      if (result.code != 0 && !result.message.empty())
      {
        print_error(result.message);
      }

      return true;
    }

    if (member == "run")
    {
      vix::reply::api::println("error: Vix.run() is disabled in Reply.");
      return true;
    }

    return false;
  }

  static bool contains_forbidden_code_chars(const std::string &s)
  {
    for (char c : s)
    {
      if (c == ';' || c == '#')
      {
        return true;
      }
    }

    return false;
  }

  static bool contains_math_ops_anywhere(const std::string &s)
  {
    for (char c : s)
    {
      if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')')
      {
        return true;
      }
    }

    return false;
  }

  static std::optional<std::pair<std::string, std::string>> parse_assignment(const std::string &line)
  {
    bool inQuote = false;
    char quote = 0;
    std::size_t eq = std::string::npos;

    for (std::size_t i = 0; i < line.size(); ++i)
    {
      char c = line[i];

      if (inQuote)
      {
        if (c == '\\' && i + 1 < line.size())
        {
          ++i;
          continue;
        }

        if (c == quote)
        {
          inQuote = false;
        }

        continue;
      }

      if (c == '"' || c == '\'')
      {
        inQuote = true;
        quote = c;
        continue;
      }

      if (c == '=')
      {
        eq = i;
        break;
      }
    }

    if (eq == std::string::npos)
    {
      return std::nullopt;
    }

    std::string left = vix::reply::trim_copy(line.substr(0, eq));
    std::string right = vix::reply::trim_copy(line.substr(eq + 1));

    if (left.empty() || right.empty())
    {
      return std::nullopt;
    }

    if (!is_ident_start(left.front()))
    {
      return std::nullopt;
    }

    for (char c : left)
    {
      if (!is_ident_char(c))
      {
        return std::nullopt;
      }
    }

    return std::make_pair(left, right);
  }

  static bool looks_like_json_literal(const std::string &rhs)
  {
    auto t = vix::reply::trim_copy(rhs);

    if (t.empty())
    {
      return false;
    }

    return t.front() == '{' || t.front() == '[';
  }

  static bool substitute_numeric_vars(
      const std::string &expr,
      const std::unordered_map<std::string, nlohmann::json> &vars,
      std::string &out,
      std::string &err)
  {
    out.clear();
    err.clear();

    bool inQuote = false;
    char quote = 0;

    for (std::size_t i = 0; i < expr.size();)
    {
      char c = expr[i];

      if (inQuote)
      {
        if (c == '\\' && i + 1 < expr.size())
        {
          out.push_back(expr[i]);
          out.push_back(expr[i + 1]);
          i += 2;
          continue;
        }

        out.push_back(c);

        if (c == quote)
        {
          inQuote = false;
        }

        ++i;
        continue;
      }

      if (c == '"' || c == '\'')
      {
        inQuote = true;
        quote = c;
        out.push_back(c);
        ++i;
        continue;
      }

      if (is_ident_start(c))
      {
        std::size_t j = i + 1;

        while (j < expr.size() && is_ident_char(expr[j]))
        {
          ++j;
        }

        std::string name = expr.substr(i, j - i);
        auto it = vars.find(name);

        if (it == vars.end())
        {
          err = "unknown variable: " + name;
          return false;
        }

        if (!it->second.is_number())
        {
          err = "variable '" + name + "' is not a number";
          return false;
        }

        out += it->second.dump();
        i = j;
        continue;
      }

      out.push_back(c);
      ++i;
    }

    return true;
  }

  static bool try_eval_math_with_vars(
      const std::string &expr,
      const std::unordered_map<std::string, nlohmann::json> &vars,
      nlohmann::json &valueOut,
      std::string &formattedOut,
      std::string &err)
  {
    err.clear();
    formattedOut.clear();

    std::string substituted;

    if (!substitute_numeric_vars(expr, vars, substituted, err))
    {
      return false;
    }

    auto result = vix::reply::eval_expression(substituted, err);

    if (!result)
    {
      return false;
    }

    formattedOut = result->formatted;

    try
    {
      const double v = std::stod(result->formatted);
      const double iv = std::round(v);

      if (std::fabs(v - iv) < 1e-12 &&
          iv >= static_cast<double>(LLONG_MIN) &&
          iv <= static_cast<double>(LLONG_MAX))
      {
        valueOut = static_cast<long long>(iv);
      }
      else
      {
        valueOut = v;
      }
    }
    catch (...)
    {
      err = "math result is not a number";
      return false;
    }

    return true;
  }

  static bool looks_like_path_expr(const std::string &s)
  {
    if (s.empty())
    {
      return false;
    }

    if (!is_ident_start(s.front()))
    {
      return false;
    }

    for (char c : s)
    {
      if (std::isspace(static_cast<unsigned char>(c)))
      {
        return false;
      }
    }

    return s.find('.') != std::string::npos || s.find('[') != std::string::npos;
  }

  static bool try_print_resolved_path_expr(
      const std::string &line,
      const std::unordered_map<std::string, nlohmann::json> &vars)
  {
    const std::string expr = vix::reply::trim_copy(line);

    if (!looks_like_path_expr(expr))
    {
      return false;
    }

    nlohmann::json out;
    std::string err;

    if (!resolve_value_at_path(vars, expr, out, err))
    {
      std::cout << "error: " << err << "\n";
      return true;
    }

    std::cout << format_json_scalar(out) << "\n";
    return true;
  }

  static bool try_parse_simple_literal(const std::string &raw, nlohmann::json &out)
  {
    const std::string t = vix::reply::trim_copy(raw);

    if (t.empty())
    {
      return false;
    }

    if (t.size() >= 2 &&
        ((t.front() == '"' && t.back() == '"') ||
         (t.front() == '\'' && t.back() == '\'')))
    {
      std::string s;
      s.reserve(t.size() - 2);

      for (std::size_t i = 1; i + 1 < t.size(); ++i)
      {
        char c = t[i];

        if (c == '\\' && i + 1 < t.size() - 1)
        {
          const char esc = t[++i];

          switch (esc)
          {
          case 'n':
            s.push_back('\n');
            break;
          case 't':
            s.push_back('\t');
            break;
          case 'r':
            s.push_back('\r');
            break;
          case '\\':
            s.push_back('\\');
            break;
          case '"':
            s.push_back('"');
            break;
          case '\'':
            s.push_back('\'');
            break;
          case '0':
            s.push_back('\0');
            break;
          default:
            s.push_back(esc);
            break;
          }

          continue;
        }

        s.push_back(c);
      }

      out = s;
      return true;
    }

    if (t == "true")
    {
      out = true;
      return true;
    }

    if (t == "false")
    {
      out = false;
      return true;
    }

    if (t == "null")
    {
      out = nullptr;
      return true;
    }

    long long iv = 0;

    if (try_parse_int_text(t, iv))
    {
      out = iv;
      return true;
    }

    double dv = 0.0;

    if (try_parse_double_text(t, dv))
    {
      out = dv;
      return true;
    }

    auto starts_with = [](const std::string &s, const std::string &prefix) -> bool
    {
      return s.rfind(prefix, 0) == 0;
    };

    if ((starts_with(t, "int(") ||
         starts_with(t, "long(") ||
         starts_with(t, "long long(")) &&
        !t.empty() &&
        t.back() == ')')
    {
      const std::size_t lp = t.find('(');
      const std::string inner = vix::reply::trim_copy(t.substr(lp + 1, t.size() - lp - 2));

      long long value = 0;

      if (try_parse_int_text(inner, value))
      {
        out = value;
        return true;
      }

      double number = 0.0;

      if (try_parse_double_text(inner, number))
      {
        out = static_cast<long long>(number);
        return true;
      }

      return false;
    }

    if ((starts_with(t, "double(") || starts_with(t, "float(")) &&
        !t.empty() &&
        t.back() == ')')
    {
      const std::size_t lp = t.find('(');
      const std::string inner = vix::reply::trim_copy(t.substr(lp + 1, t.size() - lp - 2));

      double value = 0.0;

      if (try_parse_double_text(inner, value))
      {
        out = value;
        return true;
      }

      long long integerValue = 0;

      if (try_parse_int_text(inner, integerValue))
      {
        out = static_cast<double>(integerValue);
        return true;
      }

      return false;
    }

    if (starts_with(t, "bool(") && t.back() == ')')
    {
      const std::string inner = vix::reply::trim_copy(t.substr(5, t.size() - 6));

      if (inner == "true")
      {
        out = true;
        return true;
      }

      if (inner == "false")
      {
        out = false;
        return true;
      }

      return false;
    }

    if ((starts_with(t, "string(") || starts_with(t, "std::string(")) &&
        !t.empty() &&
        t.back() == ')')
    {
      const std::size_t lp = t.find('(');
      const std::string inner = vix::reply::trim_copy(t.substr(lp + 1, t.size() - lp - 2));

      if (inner.size() >= 2 &&
          ((inner.front() == '"' && inner.back() == '"') ||
           (inner.front() == '\'' && inner.back() == '\'')))
      {
        out = inner.substr(1, inner.size() - 2);
        return true;
      }

      return false;
    }

    return false;
  }
}

namespace vix::reply
{
  int repl_flow_run(const std::vector<std::string> &replArgs)
  {
    ReplConfig cfg;
    cfg.historyFile = (user_home_dir() / ".vix_reply_history").string();

    History history(cfg.maxHistory);
    api::Vix vixObj(&history);
    std::unordered_map<std::string, nlohmann::json> vars;

    vixObj.set_args(replArgs);

    if (cfg.enableFileHistory)
    {
      std::string err;
      history.loadFromFile(cfg.historyFile, &err);
    }

    if (cfg.showBannerOnStart)
    {
      print_banner();
    }

#ifndef _WIN32
    TerminalRawMode rawMode;

    int histIndex = -1;
    std::string histDraft;
#endif

#ifndef _WIN32
    auto run_cpp_snippet_from_repl = [&](const std::vector<std::string> &lines) -> int
    {
      rawMode.disable();

      const int rc = run_cpp_snippet(lines);

      rawMode.enable();

      return rc;
    };
#else
    auto run_cpp_snippet_from_repl = [&](const std::vector<std::string> &lines) -> int
    {
      return run_cpp_snippet(lines);
    };
#endif

    bool cppMode = false;
    bool cppSeenMain = false;
    int cppBraceDepth = 0;
    std::vector<std::string> cppLines;

    while (true)
    {
      const fs::path cwd = fs::current_path();

      const std::string prompt =
          cppMode
              ? (cppLines.empty() ? "cpp> " : "...   ")
              : make_prompt(cwd);

      std::string line;

#ifndef _WIN32
      auto onHistoryUp = [&](std::string &ioLine) -> bool
      {
        const auto &items = history.items();

        if (items.empty())
        {
          return false;
        }

        if (histIndex == -1)
        {
          histDraft = ioLine;
          histIndex = static_cast<int>(items.size()) - 1;
          ioLine = items[static_cast<std::size_t>(histIndex)];
          return true;
        }

        if (histIndex > 0)
        {
          --histIndex;
          ioLine = items[static_cast<std::size_t>(histIndex)];
          return true;
        }

        return false;
      };

      auto onHistoryDown = [&](std::string &ioLine) -> bool
      {
        const auto &items = history.items();

        if (items.empty())
        {
          return false;
        }

        if (histIndex == -1)
        {
          return false;
        }

        if (histIndex < static_cast<int>(items.size()) - 1)
        {
          ++histIndex;
          ioLine = items[static_cast<std::size_t>(histIndex)];
          return true;
        }

        histIndex = -1;
        ioLine = histDraft;
        return true;
      };

      auto reset_history_browse = [&]()
      {
        histIndex = -1;
        histDraft.clear();
      };

      auto completer = [&](const std::string &current) -> CompletionResult
      {
        CompletionResult result;

        std::string trimmed = trim_copy(current);
        auto parts = split_command_line(trimmed);

        auto local_starts_with = [](const std::string &s, const std::string &p) -> bool
        {
          return s.rfind(p, 0) == 0;
        };

        const std::vector<std::string> builtins = {
            "help",
            "exit",
            "clear",
            "pwd",
            "cd",
            "history",
            "version",
            "commands",
            "calc"};

        auto options_for = [&](const std::string &) -> std::vector<std::string>
        {
          return {};
        };

        auto normalize_list = [](std::vector<std::string> &values)
        {
          std::sort(values.begin(), values.end());
          values.erase(std::unique(values.begin(), values.end()), values.end());
        };

        if (parts.size() <= 1 && trimmed.find(' ') == std::string::npos)
        {
          const std::string prefix = trimmed;
          std::vector<std::string> matches;

          for (const auto &builtin : builtins)
          {
            if (local_starts_with(builtin, prefix))
            {
              matches.push_back(builtin);
            }
          }

          normalize_list(matches);

          if (matches.size() == 1)
          {
            result.changed = true;
            result.newLine = matches[0] + " ";
            return result;
          }

          if (matches.size() > 1)
          {
            result.suggestions = matches;
            return result;
          }

          return result;
        }

        if (parts.empty())
        {
          return result;
        }

        const std::string cmd = parts[0];

        if (cmd == "cd" && trimmed.size() >= 2)
        {
          std::string pathPrefix;

          if (parts.size() >= 2)
          {
            pathPrefix = parts.back();
          }

          fs::path baseDir = ".";
          std::string filePrefix = pathPrefix;

          auto pos = pathPrefix.find_last_of("/\\");

          if (pos != std::string::npos)
          {
            baseDir = fs::path(pathPrefix.substr(0, pos + 1));
            filePrefix = pathPrefix.substr(pos + 1);
          }

          std::vector<std::string> dirs;

          std::error_code ec;

          if (fs::exists(baseDir, ec) && fs::is_directory(baseDir, ec))
          {
            for (auto &it : fs::directory_iterator(baseDir, ec))
            {
              if (ec)
              {
                break;
              }

              if (!it.is_directory(ec))
              {
                continue;
              }

              std::string name = it.path().filename().string();

              if (local_starts_with(name, filePrefix))
              {
                std::string full = (baseDir / name).string();

                if (!full.empty() && full.back() != '/')
                {
                  full.push_back('/');
                }

                dirs.push_back(full);
              }
            }
          }

          normalize_list(dirs);

          if (dirs.size() == 1)
          {
            result.changed = true;
            result.newLine = "cd " + dirs[0];
            return result;
          }

          if (dirs.size() > 1)
          {
            result.suggestions = dirs;
            return result;
          }

          return result;
        }

        const std::vector<std::string> opts = options_for(cmd);

        std::string lastToken = parts.back();

        if (!lastToken.empty() && lastToken[0] == '-')
        {
          std::vector<std::string> matches;

          for (const auto &option : opts)
          {
            if (local_starts_with(option, lastToken))
            {
              matches.push_back(option);
            }
          }

          normalize_list(matches);

          if (matches.size() == 1)
          {
            std::string rebuilt;

            for (std::size_t i = 0; i + 1 < parts.size(); ++i)
            {
              rebuilt += parts[i];
              rebuilt += " ";
            }

            rebuilt += matches[0];
            rebuilt += " ";

            result.changed = true;
            result.newLine = rebuilt;
            return result;
          }

          if (matches.size() > 1)
          {
            result.suggestions = matches;
            return result;
          }

          return result;
        }

        return result;
      };

      ReadStatus status = read_line_edit(prompt, line, completer, onHistoryUp, onHistoryDown);

      if (status == ReadStatus::Interrupted)
      {
        break;
      }

      if (status == ReadStatus::Eof)
      {
        break;
      }

      if (status == ReadStatus::Clear)
      {
        clear_screen();

        if (cfg.showBannerOnClear)
        {
          print_banner();
        }

        continue;
      }

      reset_history_browse();

      line = trim_copy(line);

      if (line.empty())
      {
        continue;
      }

      history.add(line);

      if (cppMode)
      {
        if (line == ":cancel" || line == ".cancel")
        {
          cppMode = false;
          cppSeenMain = false;
          cppBraceDepth = 0;
          cppLines.clear();

          std::cout << "C++ snippet cancelled\n";
          continue;
        }

        if (line == ":run" || line == ".run")
        {
          const int rc = run_cpp_snippet_from_repl(cppLines);

          cppMode = false;
          cppSeenMain = false;
          cppBraceDepth = 0;
          cppLines.clear();

          (void)rc;
          continue;
        }

        cppLines.push_back(line);

        if (looks_like_cpp_main(line))
        {
          cppSeenMain = true;
        }

        cppBraceDepth += count_cpp_braces_delta(line);

        if (cppSeenMain && cppBraceDepth <= 0 && line.find('}') != std::string::npos)
        {
          const int rc = run_cpp_snippet_from_repl(cppLines);

          cppMode = false;
          cppSeenMain = false;
          cppBraceDepth = 0;
          cppLines.clear();

          (void)rc;
        }

        continue;
      }

      if (line == ":cpp" || line == ".cpp")
      {
        cppMode = true;
        cppSeenMain = false;
        cppBraceDepth = 0;
        cppLines.clear();

        std::cout << "C++ mode. Type :run to execute or :cancel to exit.\n";
        continue;
      }

      if (auto assignment = parse_assignment(line))
      {
        const std::string &name = assignment->first;
        const std::string &rhs = assignment->second;

        if (looks_like_json_literal(rhs))
        {
          try
          {
            nlohmann::json j = nlohmann::json::parse(rhs);
            vars[name] = j;
            std::cout << name << " = " << j.dump() << "\n";
          }
          catch (const std::exception &e)
          {
            std::cout << "error: " << e.what() << "\n";

            auto t = trim_copy(rhs);

            if (!t.empty() && t.front() == '{')
            {
              if (t.find("\",") != std::string::npos &&
                  t.find("\":") == std::string::npos)
              {
                std::cout << "hint: JSON object uses ':' between key and value.\n";
                std::cout << "      example: {\"name\":\"Gaspard\",\"age\":10}\n";
              }
            }
          }

          continue;
        }

        nlohmann::json parsed;

        if (try_parse_simple_literal(rhs, parsed))
        {
          vars[name] = parsed;
          std::cout << name << " = " << vars[name].dump() << "\n";
          continue;
        }

        if (contains_forbidden_code_chars(rhs))
        {
          std::cout << "error: invalid expression\n";
          continue;
        }

        std::string err;
        std::string formatted;
        nlohmann::json value;

        if (!try_eval_math_with_vars(rhs, vars, value, formatted, err))
        {
          std::cout << "error: " << (err.empty() ? "invalid expression" : err) << "\n";
          continue;
        }

        vars[name] = value;
        std::cout << formatted << "\n";
        continue;
      }

      if (api::looks_like_call(line))
      {
        auto parsedCall = api::parse_call(line);

        if (parsedCall.ok)
        {
          if (invoke_call(parsedCall.expr, vixObj, vars))
          {
            if (vixObj.exit_requested())
            {
              break;
            }

            continue;
          }

          if (!parsedCall.expr.object.empty())
          {
            std::cout << "Unknown call: "
                      << parsedCall.expr.object
                      << "."
                      << parsedCall.expr.member
                      << "(...)\n";
          }
          else
          {
            std::cout << "Unknown call: "
                      << parsedCall.expr.callee
                      << "(...)\n";
          }

          continue;
        }

        std::cout << "error: " << parsedCall.error << "\n";
        continue;
      }

#else
      std::cout << prompt << std::flush;

      if (!std::getline(std::cin, line))
      {
        std::cout << "\n";
        break;
      }

      line = trim_copy(line);
#endif

      {
        std::string text = trim_copy(line);

        if (!text.empty() && is_ident_start(text.front()))
        {
          bool ok = true;

          for (char c : text)
          {
            if (!is_ident_char(c))
            {
              ok = false;
              break;
            }
          }

          if (ok)
          {
            auto it = vars.find(text);

            if (it != vars.end())
            {
              std::cout << format_json_scalar(it->second) << "\n";
              continue;
            }
          }
        }
      }

      if (try_print_resolved_path_expr(line, vars))
      {
        continue;
      }

      if (line == "exit" || line == ".exit")
      {
        break;
      }

      if (line == "help" ||
          starts_with(line, "help ") ||
          line == ".help" ||
          starts_with(line, ".help "))
      {
        auto normalized = line;

        if (starts_with(normalized, ".help"))
        {
          normalized.erase(0, 1);
        }

        auto parts = split_command_line(normalized);

        if (parts.size() == 1)
        {
          print_help();
          continue;
        }

        std::cout << "error: 'help <command>' is disabled in Reply. Use the host CLI help command instead.\n";
        continue;
      }

      if (line == "version" || line == ".version")
      {
        std::cout << "Vix Reply " << VIX_REPLY_VERSION << "\n";
        continue;
      }

      if (line == "pwd" || line == ".pwd")
      {
        std::cout << fs::current_path().string() << "\n";
        continue;
      }

      if (line == "clear" || line == ".clear")
      {
        clear_screen();

        if (cfg.showBannerOnClear)
        {
          print_banner();
        }

        continue;
      }

      if (line == "history" || line == ".history")
      {
        const auto &items = history.items();

        for (std::size_t i = 0; i < items.size(); ++i)
        {
          std::cout << (i + 1) << "  " << items[i] << "\n";
        }

        continue;
      }

      if (line == "history clear" || line == ".history clear")
      {
        history.clear();
        std::cout << "history cleared\n";
        continue;
      }

      if (line == "commands" || line == ".commands")
      {
        std::cout << "error: CLI commands are disabled in standalone Reply.\n";
        continue;
      }

      if (starts_with(line, "cd ") || starts_with(line, ".cd "))
      {
        auto normalized = line;

        if (starts_with(normalized, ".cd"))
        {
          normalized.erase(0, 1);
        }

        auto parts = split_command_line(normalized);

        if (parts.size() < 2)
        {
          std::cout << "usage: cd <dir>\n";
          continue;
        }

        std::error_code ec;
        fs::current_path(parts[1], ec);

        if (ec)
        {
          std::cout << "cd: " << ec.message() << "\n";
        }

        continue;
      }

      if (cfg.enableCalculator &&
          (starts_with(line, "calc ") || starts_with(line, ".calc ")))
      {
        auto expr = line;

        if (starts_with(expr, ".calc"))
        {
          expr.erase(0, 1);
        }

        expr = strip_prefix(expr, "calc");

        if (expr.empty())
        {
          std::cout << "usage: calc <expr>\n";
          continue;
        }

        std::string err;
        auto result = eval_expression(expr, err);

        if (!result)
        {
          std::cout << "calc error: " << err << "\n";
          continue;
        }

        std::cout << result->formatted << "\n";
        continue;
      }

      auto parts = split_command_line(line);

      if (parts.empty())
      {
        continue;
      }

      const std::string cmd = parts[0];

      if (cfg.enableCalculator &&
          !contains_forbidden_code_chars(line) &&
          contains_math_ops_anywhere(line))
      {
        std::string err;
        std::string formatted;
        nlohmann::json value;

        if (try_eval_math_with_vars(line, vars, value, formatted, err))
        {
          std::cout << formatted << "\n";
          continue;
        }

        if (!err.empty())
        {
          std::cout << "error: " << err << "\n";
          continue;
        }
      }

      std::cout << "Unknown command: " << cmd << " (type help)\n";
    }

    if (cfg.enableFileHistory)
    {
      std::string err;

      if (!history.saveToFile(cfg.historyFile, &err))
      {
        std::cout << "warning: " << err << "\n";
      }
    }

    return vixObj.exit_requested() ? vixObj.exit_code() : 0;
  }
}
