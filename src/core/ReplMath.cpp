/**
 *  @file ReplMath.cpp
 *  @brief Arithmetic expression evaluator implementation for the Vix Reply REPL.
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

#include <vix/reply/core/ReplMath.hpp>

#include <cctype>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace vix::reply
{
  namespace
  {
    enum class TokType
    {
      Num,
      Op,
      LParen,
      RParen
    };

    struct Tok
    {
      TokType type;
      double num = 0.0;
      char op = 0;
    };

    static bool is_op(char c)
    {
      return c == '+' || c == '-' || c == '*' || c == '/' || c == '%';
    }

    static int prec(char op)
    {
      if (op == '+' || op == '-')
      {
        return 1;
      }

      if (op == '*' || op == '/' || op == '%')
      {
        return 2;
      }

      return 0;
    }

    static bool tokenize(const std::string &input, std::vector<Tok> &out, std::string &err)
    {
      out.clear();

      std::size_t index = 0;

      auto skip_ws = [&]()
      {
        while (index < input.size() &&
               std::isspace(static_cast<unsigned char>(input[index])))
        {
          ++index;
        }
      };

      bool expectValue = true;

      while (true)
      {
        skip_ws();

        if (index >= input.size())
        {
          break;
        }

        const char c = input[index];

        if (c == '(')
        {
          out.push_back({TokType::LParen});
          ++index;
          expectValue = true;
          continue;
        }

        if (c == ')')
        {
          out.push_back({TokType::RParen});
          ++index;
          expectValue = false;
          continue;
        }

        if (is_op(c))
        {
          if (expectValue && (c == '+' || c == '-'))
          {
            const char sign = c;
            ++index;

            skip_ws();

            if (index >= input.size())
            {
              err = "dangling unary operator";
              return false;
            }

            if (input[index] == '(')
            {
              out.push_back({TokType::Num, 0.0, 0});
              out.push_back({TokType::Op, 0.0, sign});
              expectValue = true;
              continue;
            }

            const std::size_t start = index;
            bool dot = false;

            while (index < input.size() &&
                   (std::isdigit(static_cast<unsigned char>(input[index])) ||
                    input[index] == '.'))
            {
              if (input[index] == '.')
              {
                if (dot)
                {
                  break;
                }

                dot = true;
              }

              ++index;
            }

            if (start == index)
            {
              err = "expected number after unary operator";
              return false;
            }

            double value = 0.0;

            try
            {
              value = std::stod(input.substr(start, index - start));
            }
            catch (...)
            {
              err = "invalid number";
              return false;
            }

            if (sign == '-')
            {
              value = -value;
            }

            out.push_back({TokType::Num, value, 0});
            expectValue = false;
            continue;
          }

          out.push_back({TokType::Op, 0.0, c});
          ++index;
          expectValue = true;
          continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.')
        {
          const std::size_t start = index;
          bool dot = false;

          while (index < input.size() &&
                 (std::isdigit(static_cast<unsigned char>(input[index])) ||
                  input[index] == '.'))
          {
            if (input[index] == '.')
            {
              if (dot)
              {
                break;
              }

              dot = true;
            }

            ++index;
          }

          double value = 0.0;

          try
          {
            value = std::stod(input.substr(start, index - start));
          }
          catch (...)
          {
            err = "invalid number";
            return false;
          }

          out.push_back({TokType::Num, value, 0});
          expectValue = false;
          continue;
        }

        err = std::string("unexpected character: '") + c + "'";
        return false;
      }

      return true;
    }

    static bool to_rpn(const std::vector<Tok> &input, std::vector<Tok> &out, std::string &err)
    {
      out.clear();

      std::vector<Tok> ops;

      for (const auto &token : input)
      {
        if (token.type == TokType::Num)
        {
          out.push_back(token);
          continue;
        }

        if (token.type == TokType::Op)
        {
          while (!ops.empty())
          {
            const auto &top = ops.back();

            if (top.type == TokType::Op && prec(top.op) >= prec(token.op))
            {
              out.push_back(top);
              ops.pop_back();
              continue;
            }

            break;
          }

          ops.push_back(token);
          continue;
        }

        if (token.type == TokType::LParen)
        {
          ops.push_back(token);
          continue;
        }

        if (token.type == TokType::RParen)
        {
          bool matched = false;

          while (!ops.empty())
          {
            auto top = ops.back();
            ops.pop_back();

            if (top.type == TokType::LParen)
            {
              matched = true;
              break;
            }

            out.push_back(top);
          }

          if (!matched)
          {
            err = "mismatched ')'";
            return false;
          }

          continue;
        }
      }

      while (!ops.empty())
      {
        auto top = ops.back();
        ops.pop_back();

        if (top.type == TokType::LParen)
        {
          err = "mismatched '('";
          return false;
        }

        out.push_back(top);
      }

      return true;
    }

    static bool eval_rpn(const std::vector<Tok> &rpn, double &result, std::string &err)
    {
      std::vector<double> stack;

      for (const auto &token : rpn)
      {
        if (token.type == TokType::Num)
        {
          stack.push_back(token.num);
          continue;
        }

        if (token.type == TokType::Op)
        {
          if (stack.size() < 2)
          {
            err = "invalid expression";
            return false;
          }

          const double b = stack.back();
          stack.pop_back();

          const double a = stack.back();
          stack.pop_back();

          switch (token.op)
          {
          case '+':
            stack.push_back(a + b);
            break;

          case '-':
            stack.push_back(a - b);
            break;

          case '*':
            stack.push_back(a * b);
            break;

          case '/':
            if (b == 0.0)
            {
              err = "division by zero";
              return false;
            }

            stack.push_back(a / b);
            break;

          case '%':
            if (b == 0.0)
            {
              err = "mod by zero";
              return false;
            }

            stack.push_back(std::fmod(a, b));
            break;

          default:
            err = "unknown operator";
            return false;
          }

          continue;
        }

        err = "invalid token";
        return false;
      }

      if (stack.size() != 1)
      {
        err = "invalid expression";
        return false;
      }

      result = stack[0];
      return true;
    }

    static std::string format_double(double value)
    {
      std::ostringstream oss;
      oss.setf(std::ios::fixed);
      oss.precision(12);
      oss << value;

      std::string result = oss.str();

      while (result.size() > 1 &&
             result.find('.') != std::string::npos &&
             result.back() == '0')
      {
        result.pop_back();
      }

      if (!result.empty() && result.back() == '.')
      {
        result.pop_back();
      }

      return result;
    }
  }

  std::optional<CalcResult> eval_expression(const std::string &expr, std::string &error)
  {
    std::vector<Tok> tokens;

    if (!tokenize(expr, tokens, error))
    {
      return std::nullopt;
    }

    std::vector<Tok> rpn;

    if (!to_rpn(tokens, rpn, error))
    {
      return std::nullopt;
    }

    double output = 0.0;

    if (!eval_rpn(rpn, output, error))
    {
      return std::nullopt;
    }

    CalcResult result;
    result.value = output;
    result.formatted = format_double(output);

    return result;
  }
}
