/**
 *  @file ReplCallParser.cpp
 *  @brief Function-style call parser implementation for the Vix Reply REPL.
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

#include <vix/reply/api/ReplCallParser.hpp>

#include <cctype>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vix::reply::api
{
  namespace
  {
    enum class TokType
    {
      Ident,
      Number,
      String,
      LParen,
      RParen,
      Comma,
      Dot,
      End,
      Invalid
    };

    struct Tok
    {
      TokType type = TokType::End;
      std::string text;
    };

    static bool is_ident_start(char c)
    {
      const unsigned char u = static_cast<unsigned char>(c);
      return std::isalpha(u) || c == '_';
    }

    static bool is_ident_char(char c)
    {
      const unsigned char u = static_cast<unsigned char>(c);
      return std::isalnum(u) || c == '_';
    }

    static std::string trim_copy(std::string_view value)
    {
      std::size_t begin = 0;

      while (begin < value.size() &&
             std::isspace(static_cast<unsigned char>(value[begin])))
      {
        ++begin;
      }

      std::size_t end = value.size();

      while (end > begin &&
             std::isspace(static_cast<unsigned char>(value[end - 1])))
      {
        --end;
      }

      return std::string(value.substr(begin, end - begin));
    }

    struct Lexer
    {
      std::string_view input;
      std::size_t index = 0;

      explicit Lexer(std::string_view source)
          : input(source)
      {
      }

      void skip_ws()
      {
        while (index < input.size() &&
               std::isspace(static_cast<unsigned char>(input[index])))
        {
          ++index;
        }
      }

      Tok next()
      {
        skip_ws();

        if (index >= input.size())
        {
          return {TokType::End, ""};
        }

        const char c = input[index];

        if (c == '(')
        {
          ++index;
          return {TokType::LParen, "("};
        }

        if (c == ')')
        {
          ++index;
          return {TokType::RParen, ")"};
        }

        if (c == ',')
        {
          ++index;
          return {TokType::Comma, ","};
        }

        if (c == '.')
        {
          ++index;
          return {TokType::Dot, "."};
        }

        if (c == '"' || c == '\'')
        {
          const char quote = c;
          ++index;

          std::string out;
          bool closed = false;

          while (index < input.size())
          {
            const char ch = input[index++];

            if (ch == quote)
            {
              closed = true;
              break;
            }

            if (ch == '\\')
            {
              if (index >= input.size())
              {
                return {TokType::Invalid, "unterminated escape sequence"};
              }

              const char esc = input[index++];

              switch (esc)
              {
              case 'n':
                out.push_back('\n');
                break;
              case 't':
                out.push_back('\t');
                break;
              case 'r':
                out.push_back('\r');
                break;
              case '\\':
                out.push_back('\\');
                break;
              case '"':
                out.push_back('"');
                break;
              case '\'':
                out.push_back('\'');
                break;
              case '0':
                out.push_back('\0');
                break;
              default:
                out.push_back(esc);
                break;
              }

              continue;
            }

            out.push_back(ch);
          }

          if (!closed)
          {
            return {TokType::Invalid, "unterminated string literal"};
          }

          return {TokType::String, out};
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            ((c == '+' || c == '-') &&
             (index + 1) < input.size() &&
             (std::isdigit(static_cast<unsigned char>(input[index + 1])) ||
              input[index + 1] == '.')) ||
            (c == '.' &&
             (index + 1) < input.size() &&
             std::isdigit(static_cast<unsigned char>(input[index + 1]))))
        {
          const std::size_t start = index;

          if (input[index] == '+' || input[index] == '-')
          {
            ++index;
          }

          bool hasDot = false;
          bool hasDigit = false;

          while (index < input.size())
          {
            const char ch = input[index];

            if (std::isdigit(static_cast<unsigned char>(ch)))
            {
              hasDigit = true;
              ++index;
              continue;
            }

            if (ch == '.' && !hasDot)
            {
              hasDot = true;
              ++index;
              continue;
            }

            break;
          }

          if (!hasDigit)
          {
            return {TokType::Invalid, "invalid number"};
          }

          return {TokType::Number, std::string(input.substr(start, index - start))};
        }

        if (is_ident_start(c))
        {
          const std::size_t start = index++;

          while (index < input.size() && is_ident_char(input[index]))
          {
            ++index;
          }

          return {TokType::Ident, std::string(input.substr(start, index - start))};
        }

        ++index;
        return {TokType::Invalid, std::string("unexpected character: '") + c + "'"};
      }
    };

    static bool parse_int_strict(const std::string &text, long long &out)
    {
      try
      {
        std::size_t idx = 0;
        const long long value = std::stoll(text, &idx, 10);

        if (idx != text.size())
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

    static bool parse_double_strict(const std::string &text, double &out)
    {
      try
      {
        std::size_t idx = 0;
        const double value = std::stod(text, &idx);

        if (idx != text.size())
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

    static CallValue make_literal_from_ident(const std::string &ident, bool *ok)
    {
      *ok = true;

      if (ident == "true")
      {
        return {true};
      }

      if (ident == "false")
      {
        return {false};
      }

      if (ident == "null")
      {
        return {std::monostate{}};
      }

      *ok = false;
      return {std::monostate{}};
    }

    static bool parse_value(Lexer &lexer, Tok &current, CallValue &out, std::string &err)
    {
      if (current.type == TokType::Invalid)
      {
        err = current.text.empty() ? "invalid token" : current.text;
        return false;
      }

      if (current.type == TokType::String)
      {
        out.v = current.text;
        current = lexer.next();
        return true;
      }

      if (current.type == TokType::Number)
      {
        const std::string &text = current.text;
        const bool hasDot = text.find('.') != std::string::npos;

        if (hasDot)
        {
          double value = 0.0;

          if (!parse_double_strict(text, value))
          {
            err = "invalid number";
            return false;
          }

          out.v = value;
        }
        else
        {
          long long value = 0;

          if (!parse_int_strict(text, value))
          {
            err = "invalid integer";
            return false;
          }

          out.v = value;
        }

        current = lexer.next();
        return true;
      }

      if (current.type == TokType::Ident)
      {
        bool ok = false;
        CallValue literal = make_literal_from_ident(current.text, &ok);

        if (!ok)
        {
          err = "unexpected identifier '" + current.text + "'";
          return false;
        }

        out = std::move(literal);
        current = lexer.next();
        return true;
      }

      err = "expected value";
      return false;
    }

    static bool read_raw_arg_slice(
        std::string_view input,
        std::size_t &pos,
        std::string &out,
        std::string &err)
    {
      const std::size_t size = input.size();

      auto skip_ws = [&]()
      {
        while (pos < size &&
               std::isspace(static_cast<unsigned char>(input[pos])))
        {
          ++pos;
        }
      };

      skip_ws();

      if (pos >= size)
      {
        err = "expected value";
        return false;
      }

      const std::size_t start = pos;

      int parenDepth = 0;
      bool inQuote = false;
      char quote = 0;

      while (pos < size)
      {
        const char c = input[pos];

        if (inQuote)
        {
          if (c == '\\')
          {
            if ((pos + 1) < size)
            {
              pos += 2;
              continue;
            }

            err = "unterminated escape sequence";
            return false;
          }

          if (c == quote)
          {
            inQuote = false;
            ++pos;
            continue;
          }

          ++pos;
          continue;
        }

        if (c == '"' || c == '\'')
        {
          inQuote = true;
          quote = c;
          ++pos;
          continue;
        }

        if (c == '(')
        {
          ++parenDepth;
          ++pos;
          continue;
        }

        if (c == ')')
        {
          if (parenDepth == 0)
          {
            break;
          }

          --parenDepth;
          ++pos;
          continue;
        }

        if (c == ',' && parenDepth == 0)
        {
          break;
        }

        ++pos;
      }

      if (inQuote)
      {
        err = "unterminated string literal";
        return false;
      }

      if (parenDepth != 0)
      {
        err = "unbalanced parentheses in argument";
        return false;
      }

      out = trim_copy(input.substr(start, pos - start));

      if (out.empty())
      {
        err = "expected value";
        return false;
      }

      return true;
    }

    static std::optional<std::pair<std::string, std::string>> split_typed_call(std::string_view raw)
    {
      const std::string trimmed = trim_copy(raw);

      if (trimmed.empty() || trimmed.back() != ')')
      {
        return std::nullopt;
      }

      const std::size_t lp = trimmed.find('(');

      if (lp == std::string::npos || lp == 0)
      {
        return std::nullopt;
      }

      const std::string type = trim_copy(std::string_view(trimmed).substr(0, lp));
      const std::string inner = trim_copy(std::string_view(trimmed).substr(lp + 1, trimmed.size() - lp - 2));

      if (type.empty() || inner.empty())
      {
        return std::nullopt;
      }

      return std::make_pair(type, inner);
    }

    static bool parse_cpp_typed_literal(const std::string &raw, CallValue &out, std::string &err)
    {
      const auto typed = split_typed_call(raw);

      if (!typed)
      {
        return false;
      }

      const std::string &type = typed->first;
      const std::string &inner = typed->second;

      auto parse_inner_as_value = [&](CallValue &dst) -> bool
      {
        Lexer innerLexer(inner);
        Tok innerCurrent = innerLexer.next();

        if (!parse_value(innerLexer, innerCurrent, dst, err))
        {
          return false;
        }

        if (innerCurrent.type != TokType::End)
        {
          err = "invalid typed literal";
          return false;
        }

        return true;
      };

      if (type == "int" || type == "long" || type == "long long")
      {
        CallValue tmp;

        if (!parse_inner_as_value(tmp))
        {
          return false;
        }

        if (tmp.is_int())
        {
          out.v = tmp.as_int();
          return true;
        }

        if (tmp.is_double())
        {
          const double value = tmp.as_double();

          if (value < static_cast<double>(std::numeric_limits<long long>::min()) ||
              value > static_cast<double>(std::numeric_limits<long long>::max()))
          {
            err = type + " out of range";
            return false;
          }

          out.v = static_cast<long long>(value);
          return true;
        }

        err = type + " expects a numeric value";
        return false;
      }

      if (type == "double" || type == "float")
      {
        CallValue tmp;

        if (!parse_inner_as_value(tmp))
        {
          return false;
        }

        if (tmp.is_double())
        {
          out.v = tmp.as_double();
          return true;
        }

        if (tmp.is_int())
        {
          out.v = static_cast<double>(tmp.as_int());
          return true;
        }

        err = type + " expects a numeric value";
        return false;
      }

      if (type == "bool")
      {
        CallValue tmp;

        if (!parse_inner_as_value(tmp))
        {
          return false;
        }

        if (!tmp.is_bool())
        {
          err = "bool expects true or false";
          return false;
        }

        out.v = tmp.as_bool();
        return true;
      }

      if (type == "string" || type == "std::string")
      {
        CallValue tmp;

        if (!parse_inner_as_value(tmp))
        {
          return false;
        }

        if (!tmp.is_string())
        {
          err = type + " expects a string literal";
          return false;
        }

        out.v = tmp.as_string();
        return true;
      }

      return false;
    }

    static bool parse_single_arg_value_from_raw(const std::string &raw, CallValue &out, std::string &err)
    {
      Lexer lexer(raw);
      Tok current = lexer.next();

      std::string firstErr;

      if (parse_value(lexer, current, out, firstErr) &&
          current.type == TokType::End)
      {
        return true;
      }

      std::string typedErr;

      if (parse_cpp_typed_literal(raw, out, typedErr))
      {
        return true;
      }

      if (!typedErr.empty())
      {
        err = typedErr;
      }
      else if (!firstErr.empty())
      {
        err = firstErr;
      }
      else
      {
        err = "invalid value";
      }

      return false;
    }

    static bool validate_no_trailing_tokens(
        [[maybe_unused]] Lexer &lexer,
        Tok &current,
        std::string &err)
    {
      if (current.type == TokType::Invalid)
      {
        err = current.text.empty() ? "invalid token" : current.text;
        return false;
      }

      while (current.type != TokType::End)
      {
        if (current.type == TokType::Invalid)
        {
          err = current.text.empty() ? "invalid token" : current.text;
          return false;
        }

        err = "unexpected trailing input";
        return false;
      }

      return true;
    }
  }

  bool looks_like_call(std::string_view input)
  {
    const std::size_t lp = input.find('(');
    const std::size_t rp = input.rfind(')');

    if (lp == std::string_view::npos ||
        rp == std::string_view::npos ||
        rp < lp)
    {
      return false;
    }

    for (std::size_t i = 0; i < lp; ++i)
    {
      if (!std::isspace(static_cast<unsigned char>(input[i])))
      {
        return true;
      }
    }

    return false;
  }

  CallParseResult parse_call(std::string_view input)
  {
    CallParseResult result;
    Lexer lexer(input);

    Tok current = lexer.next();

    if (current.type == TokType::Invalid)
    {
      result.ok = false;
      result.error = current.text.empty() ? "invalid token" : current.text;
      return result;
    }

    if (current.type != TokType::Ident)
    {
      result.ok = false;
      result.error = "call must start with identifier";
      return result;
    }

    const std::string first = current.text;
    current = lexer.next();

    if (current.type == TokType::Dot)
    {
      current = lexer.next();

      if (current.type != TokType::Ident)
      {
        result.ok = false;
        result.error = "expected member name after '.'";
        return result;
      }

      result.expr.object = first;
      result.expr.member = current.text;
      current = lexer.next();
    }
    else
    {
      result.expr.callee = first;
    }

    if (current.type != TokType::LParen)
    {
      result.ok = false;
      result.error = "expected '('";
      return result;
    }

    const std::size_t lp = input.find('(');

    if (lp == std::string_view::npos)
    {
      result.ok = false;
      result.error = "expected '('";
      return result;
    }

    std::size_t rawPos = lp + 1;
    current = lexer.next();

    if (current.type == TokType::RParen)
    {
      current = lexer.next();

      if (!validate_no_trailing_tokens(lexer, current, result.error))
      {
        result.ok = false;
        return result;
      }

      result.ok = true;
      return result;
    }

    while (true)
    {
      std::string raw;
      std::string rawErr;

      if (!read_raw_arg_slice(input, rawPos, raw, rawErr))
      {
        result.ok = false;
        result.error = rawErr;
        return result;
      }

      CallValue value;
      std::string parseErr;

      if (!parse_single_arg_value_from_raw(raw, value, parseErr))
      {
        value.v = std::monostate{};
      }

      result.expr.args.push_back(std::move(value));
      result.expr.args_raw.push_back(std::move(raw));

      while (rawPos < input.size() &&
             std::isspace(static_cast<unsigned char>(input[rawPos])))
      {
        ++rawPos;
      }

      if (rawPos >= input.size())
      {
        result.ok = false;
        result.error = "expected ')'";
        return result;
      }

      if (input[rawPos] == ',')
      {
        ++rawPos;
        continue;
      }

      if (input[rawPos] == ')')
      {
        ++rawPos;
        break;
      }

      result.ok = false;
      result.error = "expected ',' or ')'";
      return result;
    }

    Lexer tailLexer(input.substr(rawPos));
    Tok tail = tailLexer.next();

    if (tail.type != TokType::End)
    {
      result.ok = false;
      result.error = (tail.type == TokType::Invalid && !tail.text.empty())
                         ? tail.text
                         : "unexpected trailing input after call";
      return result;
    }

    result.ok = true;
    return result;
  }
}
