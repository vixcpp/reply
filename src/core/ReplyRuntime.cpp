/**
 *  @file ReplyRuntime.cpp
 *  @brief Non-interactive Reply runtime implementation.
 *
 *  Vix Reply
 */

#include <vix/reply/core/ReplyRuntime.hpp>

#include <vix/reply/api/ReplCallParser.hpp>
#include <vix/reply/core/ReplMath.hpp>
#include <vix/reply/core/ReplUtils.hpp>

#include <chrono>
#include <cmath>
#include <climits>
#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifndef VIX_REPLY_VERSION
#define VIX_REPLY_VERSION "dev"
#endif

namespace fs = std::filesystem;

namespace vix::reply
{
  namespace
  {
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
      if (s.empty() || !is_ident_start(s.front()))
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

    static bool contains_math_ops(const std::string &s)
    {
      for (char c : s)
      {
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '(' || c == ')')
        {
          return true;
        }
      }

      return false;
    }

    static bool try_parse_int_text(const std::string &s, long long &out)
    {
      try
      {
        std::size_t idx = 0;
        const long long value = std::stoll(s, &idx, 10);

        if (idx != s.size())
        {
          return false;
        }

        out = value;
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
        const double value = std::stod(s, &idx);

        if (idx != s.size())
        {
          return false;
        }

        out = value;
        return true;
      }
      catch (...)
      {
        return false;
      }
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

    static bool json_number_to_long_long(const nlohmann::json &j, long long &out)
    {
      if (j.is_number_integer())
      {
        out = j.get<long long>();
        return true;
      }

      if (j.is_number_unsigned())
      {
        const auto value = j.get<unsigned long long>();

        if (value > static_cast<unsigned long long>(LLONG_MAX))
        {
          return false;
        }

        out = static_cast<long long>(value);
        return true;
      }

      if (j.is_number_float())
      {
        const double value = j.get<double>();
        const double rounded = std::round(value);

        if (std::fabs(value - rounded) < 1e-12 &&
            rounded >= static_cast<double>(LLONG_MIN) &&
            rounded <= static_cast<double>(LLONG_MAX))
        {
          out = static_cast<long long>(rounded);
          return true;
        }
      }

      return false;
    }

    static bool resolve_value_at_path(
        const std::unordered_map<std::string, nlohmann::json> &vars,
        const std::string &raw,
        nlohmann::json &out,
        std::string &err)
    {
      err.clear();

      const std::string expr = trim_copy(raw);

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

          const std::size_t start = i;

          if (i < expr.size() && (expr[i] == '+' || expr[i] == '-'))
          {
            ++i;
          }

          while (i < expr.size() && std::isdigit(static_cast<unsigned char>(expr[i])))
          {
            ++i;
          }

          const std::string indexText = expr.substr(start, i - start);

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

          if (!try_parse_int_text(indexText, index))
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

    static bool looks_like_path_expr(const std::string &s)
    {
      if (s.empty() || !is_ident_start(s.front()))
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
        const char c = expr[i];

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

          const std::string name = expr.substr(i, j - i);
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

      auto result = eval_expression(substituted, err);

      if (!result)
      {
        return false;
      }

      formattedOut = result->formatted;

      double doubleValue = 0.0;

      if (!try_parse_double_text(result->formatted, doubleValue))
      {
        err = "math result is not a number";
        return false;
      }

      const double integerValue = std::round(doubleValue);

      if (std::fabs(doubleValue - integerValue) < 1e-12 &&
          integerValue >= static_cast<double>(LLONG_MIN) &&
          integerValue <= static_cast<double>(LLONG_MAX))
      {
        valueOut = static_cast<long long>(integerValue);
      }
      else
      {
        valueOut = doubleValue;
      }

      return true;
    }

    static bool try_parse_simple_literal(const std::string &raw, nlohmann::json &out)
    {
      const std::string text = trim_copy(raw);

      if (text.empty())
      {
        return false;
      }

      if (text == "true")
      {
        out = true;
        return true;
      }

      if (text == "false")
      {
        out = false;
        return true;
      }

      if (text == "null")
      {
        out = nullptr;
        return true;
      }

      if (text.size() >= 2 &&
          ((text.front() == '"' && text.back() == '"') ||
           (text.front() == '\'' && text.back() == '\'')))
      {
        std::string value;
        value.reserve(text.size() - 2);

        for (std::size_t i = 1; i + 1 < text.size(); ++i)
        {
          char c = text[i];

          if (c == '\\' && i + 1 < text.size() - 1)
          {
            const char esc = text[++i];

            switch (esc)
            {
            case 'n':
              value.push_back('\n');
              break;
            case 't':
              value.push_back('\t');
              break;
            case 'r':
              value.push_back('\r');
              break;
            case '\\':
              value.push_back('\\');
              break;
            case '"':
              value.push_back('"');
              break;
            case '\'':
              value.push_back('\'');
              break;
            default:
              value.push_back(esc);
              break;
            }

            continue;
          }

          value.push_back(c);
        }

        out = value;
        return true;
      }

      if (text.front() == '{' || text.front() == '[')
      {
        try
        {
          out = nlohmann::json::parse(text);
          return true;
        }
        catch (...)
        {
          return false;
        }
      }

      long long intValue = 0;

      if (try_parse_int_text(text, intValue))
      {
        out = intValue;
        return true;
      }

      double doubleValue = 0.0;

      if (try_parse_double_text(text, doubleValue))
      {
        out = doubleValue;
        return true;
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
        const char c = line[i];

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

      std::string left = trim_copy(line.substr(0, eq));
      std::string right = trim_copy(line.substr(eq + 1));

      if (!looks_like_ident(left) || right.empty())
      {
        return std::nullopt;
      }

      return std::make_pair(left, right);
    }

    static nlohmann::json call_value_to_json(const api::CallValue &value)
    {
      if (value.is_string())
      {
        return value.as_string();
      }

      if (value.is_bool())
      {
        return value.as_bool();
      }

      if (value.is_int())
      {
        return value.as_int();
      }

      if (value.is_double())
      {
        return value.as_double();
      }

      return nullptr;
    }

    static bool resolve_arg_json(
        const api::CallExpr &call,
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

      if (!call.args[i].is_null())
      {
        out = call_value_to_json(call.args[i]);
        return true;
      }

      if (i < call.args_raw.size())
      {
        const std::string raw = trim_copy(call.args_raw[i]);

        if (looks_like_ident(raw) || looks_like_path_expr(raw))
        {
          if (resolve_value_at_path(vars, raw, out, err))
          {
            return true;
          }

          return false;
        }

        if (try_parse_simple_literal(raw, out))
        {
          return true;
        }

        if (contains_math_ops(raw))
        {
          std::string formatted;

          if (try_eval_math_with_vars(raw, vars, out, formatted, err))
          {
            return true;
          }

          return false;
        }
      }

      out = nullptr;
      return true;
    }

    static std::string arg_to_text(
        const api::CallExpr &call,
        std::size_t i,
        const std::unordered_map<std::string, nlohmann::json> &vars,
        std::string &err)
    {
      nlohmann::json value;

      if (!resolve_arg_json(call, i, vars, value, err))
      {
        return {};
      }

      return format_json_scalar(value);
    }
  }

  class ReplyRuntime::Impl
  {
  public:
    explicit Impl(ReplyRuntimeOptions options)
        : options_(options)
    {
    }

    ReplyRunResult run_line(std::string_view input)
    {
      const auto start = std::chrono::steady_clock::now();

      ReplyRunResult result;
      std::string line = trim_copy(std::string(input));

      auto finish = [&]()
      {
        const auto end = std::chrono::steady_clock::now();

        result.elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        return result;
      };

      if (line.empty())
      {
        return finish();
      }

      if (line == "exit" || line == ".exit")
      {
        if (!options_.allowProcessExit)
        {
          result.ok = false;
          result.errorText = "Vix.exit() is disabled in embedded Reply runtime";
          result.stderrText = "error: " + result.errorText + "\n";
          return finish();
        }

        result.exitRequested = true;
        result.exitCode = 0;
        return finish();
      }

      if (line == "version" || line == ".version")
      {
        result.valueText = VIX_REPLY_VERSION;
        result.stdoutText = result.valueText + "\n";
        return finish();
      }

      if (line == "pwd" || line == ".pwd")
      {
        result.valueText = fs::current_path().string();
        result.stdoutText = result.valueText + "\n";
        return finish();
      }

      if (line == "clear" || line == ".clear" ||
          line == "history" || line == ".history" ||
          line == "history clear" || line == ".history clear" ||
          line == "commands" || line == ".commands" ||
          line == "help" || line == ".help")
      {
        result.ok = false;
        result.errorText = "interactive command is disabled in embedded Reply runtime";
        result.stderrText = "error: " + result.errorText + "\n";
        return finish();
      }

      if (starts_with(line, ":cpp") ||
          starts_with(line, ":run") ||
          starts_with(line, ":cancel"))
      {
        result.ok = false;
        result.errorText = "C++ snippet mode is disabled in embedded Reply runtime";
        result.stderrText = "error: " + result.errorText + "\n";
        return finish();
      }

      if (options_.enableCalculator &&
          (starts_with(line, "calc ") || starts_with(line, ".calc ") ||
           starts_with(line, "= ")))
      {
        std::string expr = line;

        if (starts_with(expr, ".calc"))
        {
          expr.erase(0, 1);
        }

        if (starts_with(expr, "calc"))
        {
          expr = trim_copy(expr.substr(4));
        }
        else if (starts_with(expr, "="))
        {
          expr = trim_copy(expr.substr(1));
        }

        nlohmann::json value;
        std::string formatted;
        std::string err;

        if (!try_eval_math_with_vars(expr, vars_, value, formatted, err))
        {
          result.ok = false;
          result.errorText = err;
          result.stderrText = "error: " + err + "\n";
          return finish();
        }

        result.valueText = formatted;
        result.stdoutText = formatted + "\n";
        return finish();
      }

      if (auto assignment = parse_assignment(line))
      {
        nlohmann::json value;
        std::string formatted;
        std::string err;

        const std::string &name = assignment->first;
        const std::string &rhs = assignment->second;

        if (try_parse_simple_literal(rhs, value))
        {
          vars_[name] = value;
          result.valueText = format_json_scalar(value);
          return finish();
        }

        if (looks_like_ident(rhs) || looks_like_path_expr(rhs))
        {
          if (!resolve_value_at_path(vars_, rhs, value, err))
          {
            result.ok = false;
            result.errorText = err;
            result.stderrText = "error: " + err + "\n";
            return finish();
          }

          vars_[name] = value;
          result.valueText = format_json_scalar(value);
          return finish();
        }

        if (contains_math_ops(rhs))
        {
          if (!try_eval_math_with_vars(rhs, vars_, value, formatted, err))
          {
            result.ok = false;
            result.errorText = err;
            result.stderrText = "error: " + err + "\n";
            return finish();
          }

          vars_[name] = value;
          result.valueText = formatted;
          return finish();
        }

        result.ok = false;
        result.errorText = "cannot assign unsupported expression";
        result.stderrText = "error: " + result.errorText + "\n";
        return finish();
      }

      if (looks_like_ident(line))
      {
        auto it = vars_.find(line);

        if (it != vars_.end())
        {
          result.valueText = format_json_scalar(it->second);
          result.stdoutText = result.valueText + "\n";
          return finish();
        }
      }

      if (looks_like_path_expr(line))
      {
        nlohmann::json value;
        std::string err;

        if (!resolve_value_at_path(vars_, line, value, err))
        {
          result.ok = false;
          result.errorText = err;
          result.stderrText = "error: " + err + "\n";
          return finish();
        }

        result.valueText = format_json_scalar(value);
        result.stdoutText = result.valueText + "\n";
        return finish();
      }

      if (api::looks_like_call(line))
      {
        auto parsed = api::parse_call(line);

        if (!parsed.ok)
        {
          result.ok = false;
          result.errorText = parsed.error;
          result.stderrText = "error: " + parsed.error + "\n";
          return finish();
        }

        if (!invoke_call(parsed.expr, result))
        {
          result.ok = false;
          result.errorText = "unknown function call";
          result.stderrText = "error: " + result.errorText + "\n";
        }

        return finish();
      }

      if (contains_math_ops(line))
      {
        nlohmann::json value;
        std::string formatted;
        std::string err;

        if (!try_eval_math_with_vars(line, vars_, value, formatted, err))
        {
          result.ok = false;
          result.errorText = err;
          result.stderrText = "error: " + err + "\n";
          return finish();
        }

        result.valueText = formatted;
        result.stdoutText = formatted + "\n";
        return finish();
      }

      nlohmann::json literal;

      if (try_parse_simple_literal(line, literal))
      {
        result.valueText = format_json_scalar(literal);
        result.stdoutText = result.valueText + "\n";
        return finish();
      }

      result.ok = false;
      result.errorText = "unknown Reply input";
      result.stderrText = "error: " + result.errorText + "\n";
      return finish();
    }

    ReplyRunResult run_cell(std::string_view source)
    {
      const auto start = std::chrono::steady_clock::now();

      ReplyRunResult finalResult;

      std::stringstream ss{std::string(source)};
      std::string line;

      while (std::getline(ss, line))
      {
        auto lineResult = run_line(line);

        finalResult.stdoutText += lineResult.stdoutText;
        finalResult.stderrText += lineResult.stderrText;
        finalResult.valueText = lineResult.valueText;

        if (!lineResult.ok)
        {
          finalResult.ok = false;
          finalResult.errorText = lineResult.errorText;
          finalResult.exitCode = lineResult.exitCode;
          break;
        }

        if (lineResult.exitRequested)
        {
          finalResult.exitRequested = true;
          finalResult.exitCode = lineResult.exitCode;
          break;
        }
      }

      const auto end = std::chrono::steady_clock::now();

      finalResult.elapsedMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

      return finalResult;
    }

    void set_args(std::vector<std::string> args)
    {
      args_ = std::move(args);
    }

    void clear()
    {
      vars_.clear();
    }

  private:
    bool invoke_call(const api::CallExpr &call, ReplyRunResult &result)
    {
      auto print_error = [&](const std::string &message)
      {
        result.ok = false;
        result.errorText = message;
        result.stderrText += "error: " + message + "\n";
      };

      auto join_args = [&](std::string &err) -> std::optional<std::string>
      {
        std::string out;

        for (std::size_t i = 0; i < call.args.size(); ++i)
        {
          if (i != 0)
          {
            out += " ";
          }

          out += arg_to_text(call, i, vars_, err);

          if (!err.empty())
          {
            return std::nullopt;
          }
        }

        return out;
      };

      if (call.object.empty())
      {
        const std::string &fn = call.callee;

        if (fn == "print" || fn == "println" || fn == "eprint" || fn == "eprintln")
        {
          std::string err;
          auto text = join_args(err);

          if (!text)
          {
            print_error(err.empty() ? "invalid arguments" : err);
            return true;
          }

          if (fn == "print")
          {
            result.stdoutText += *text;
          }
          else if (fn == "println")
          {
            result.stdoutText += *text + "\n";
          }
          else if (fn == "eprint")
          {
            result.stderrText += *text;
          }
          else
          {
            result.stderrText += *text + "\n";
          }

          result.valueText = *text;
          return true;
        }

        if (fn == "cwd")
        {
          if (!call.args.empty())
          {
            print_error("cwd() takes no arguments");
            return true;
          }

          result.valueText = fs::current_path().string();
          result.stdoutText += result.valueText + "\n";
          return true;
        }

        if (fn == "pid")
        {
          if (!call.args.empty())
          {
            print_error("pid() takes no arguments");
            return true;
          }

#if defined(_WIN32)
          result.valueText = "0";
#else
          result.valueText = std::to_string(static_cast<long long>(::getpid()));
#endif
          result.stdoutText += result.valueText + "\n";
          return true;
        }

        if (fn == "len")
        {
          if (call.args.size() != 1)
          {
            print_error("len(value)");
            return true;
          }

          nlohmann::json value;
          std::string err;

          if (!resolve_arg_json(call, 0, vars_, value, err))
          {
            print_error(err);
            return true;
          }

          if (value.is_string())
          {
            result.valueText = std::to_string(value.get<std::string>().size());
          }
          else if (value.is_array() || value.is_object())
          {
            result.valueText = std::to_string(value.size());
          }
          else
          {
            print_error("len() expects string, array or object");
            return true;
          }

          result.stdoutText += result.valueText + "\n";
          return true;
        }

        if (fn == "str" || fn == "type" || fn == "int" || fn == "float" || fn == "double")
        {
          if (call.args.size() != 1)
          {
            print_error(fn + "(value)");
            return true;
          }

          nlohmann::json value;
          std::string err;

          if (!resolve_arg_json(call, 0, vars_, value, err))
          {
            print_error(err);
            return true;
          }

          if (fn == "str")
          {
            result.valueText = format_json_scalar(value);
          }
          else if (fn == "type")
          {
            if (value.is_string())
              result.valueText = "string";
            else if (value.is_boolean())
              result.valueText = "bool";
            else if (value.is_number_integer() || value.is_number_unsigned())
              result.valueText = "int";
            else if (value.is_number_float())
              result.valueText = "double";
            else if (value.is_array())
              result.valueText = "array";
            else if (value.is_object())
              result.valueText = "object";
            else if (value.is_null())
              result.valueText = "null";
            else
              result.valueText = "unknown";
          }
          else if (fn == "int")
          {
            long long out = 0;

            if (value.is_string())
            {
              if (!try_parse_int_text(value.get<std::string>(), out))
              {
                print_error("cannot convert string to int");
                return true;
              }
            }
            else if (!json_number_to_long_long(value, out))
            {
              print_error("int() expects numeric value or numeric string");
              return true;
            }

            result.valueText = std::to_string(out);
          }
          else
          {
            double out = 0.0;

            if (value.is_string())
            {
              if (!try_parse_double_text(value.get<std::string>(), out))
              {
                print_error("cannot convert string to float");
                return true;
              }
            }
            else if (!value.is_number())
            {
              print_error("float() expects numeric value or numeric string");
              return true;
            }
            else
            {
              out = value.get<double>();
            }

            result.valueText = std::to_string(out);
          }

          result.stdoutText += result.valueText + "\n";
          return true;
        }

        return false;
      }

      if (call.object != "Vix" && call.object != "vix")
      {
        return false;
      }

      const std::string &member = call.member;

      if (member == "cwd")
      {
        if (!call.args.empty())
        {
          print_error("Vix.cwd() takes no arguments");
          return true;
        }

        result.valueText = fs::current_path().string();
        result.stdoutText += result.valueText + "\n";
        return true;
      }

      if (member == "env")
      {
        if (call.args.size() != 1 || !call.args[0].is_string())
        {
          print_error("Vix.env(key:string)");
          return true;
        }

        auto value = get_env(call.args[0].as_string());

        result.valueText = value ? *value : "null";
        result.stdoutText += result.valueText + "\n";
        return true;
      }

      if (member == "args")
      {
        if (!call.args.empty())
        {
          print_error("Vix.args() takes no arguments");
          return true;
        }

        nlohmann::json array = nlohmann::json::array();

        for (const auto &arg : args_)
        {
          array.push_back(arg);
        }

        result.valueText = array.dump();
        result.stdoutText += result.valueText + "\n";
        return true;
      }

      if (member == "pid")
      {
        if (!call.args.empty())
        {
          print_error("Vix.pid() takes no arguments");
          return true;
        }

#if defined(_WIN32)
        result.valueText = "0";
#else
        result.valueText = std::to_string(static_cast<long long>(::getpid()));
#endif
        result.stdoutText += result.valueText + "\n";
        return true;
      }

      if (member == "cd")
      {
        if (!options_.allowWorkingDirectoryChange)
        {
          print_error("Vix.cd() is disabled in embedded Reply runtime");
          return true;
        }

        if (call.args.size() != 1 || !call.args[0].is_string())
        {
          print_error("Vix.cd(path:string)");
          return true;
        }

        std::error_code ec;
        fs::current_path(call.args[0].as_string(), ec);

        if (ec)
        {
          print_error("cd: " + ec.message());
        }

        return true;
      }

      if (member == "mkdir")
      {
        if (!options_.allowFilesystemWrite)
        {
          print_error("Vix.mkdir() is disabled in embedded Reply runtime");
          return true;
        }

        print_error("Vix.mkdir() is not implemented in embedded runtime yet");
        return true;
      }

      if (member == "exit")
      {
        if (!options_.allowProcessExit)
        {
          print_error("Vix.exit() is disabled in embedded Reply runtime");
          return true;
        }

        result.exitRequested = true;
        result.exitCode = 0;
        return true;
      }

      if (member == "history" || member == "history_clear")
      {
        print_error("Vix.history() is disabled in embedded Reply runtime");
        return true;
      }

      if (member == "run")
      {
        print_error("Vix.run() is disabled in Reply");
        return true;
      }

      return false;
    }

  private:
    ReplyRuntimeOptions options_;
    std::unordered_map<std::string, nlohmann::json> vars_;
    std::vector<std::string> args_;
  };

  ReplyRuntime::ReplyRuntime(ReplyRuntimeOptions options)
      : impl_(std::make_unique<Impl>(options))
  {
  }

  ReplyRuntime::~ReplyRuntime() = default;

  ReplyRuntime::ReplyRuntime(ReplyRuntime &&) noexcept = default;

  ReplyRuntime &ReplyRuntime::operator=(ReplyRuntime &&) noexcept = default;

  ReplyRunResult ReplyRuntime::run_line(std::string_view line)
  {
    return impl_->run_line(line);
  }

  ReplyRunResult ReplyRuntime::run_cell(std::string_view source)
  {
    return impl_->run_cell(source);
  }

  void ReplyRuntime::set_args(std::vector<std::string> args)
  {
    impl_->set_args(std::move(args));
  }

  void ReplyRuntime::clear()
  {
    impl_->clear();
  }
}
