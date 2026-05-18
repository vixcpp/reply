/**
 *  @file ReplCallParser.hpp
 *  @brief Parser for function-style calls inside the Vix Reply REPL.
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

#ifndef VIX_REPLY_API_REPL_CALL_PARSER_HPP
#define VIX_REPLY_API_REPL_CALL_PARSER_HPP

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace vix::reply::api
{
  /**
   * @brief Represents a parsed argument value passed to a REPL call.
   *
   * CallValue supports the small set of literal values accepted by the
   * function-style REPL syntax:
   * - null
   * - double
   * - integer
   * - boolean
   * - string
   */
  struct CallValue
  {
    /**
     * @brief Internal value storage.
     */
    using Value = std::variant<std::monostate, double, long long, bool, std::string>;

    /**
     * @brief Stored argument value.
     */
    Value v;

    /**
     * @brief Checks whether the value is null.
     *
     * @return True if the value is null, false otherwise.
     */
    bool is_null() const
    {
      return std::holds_alternative<std::monostate>(v);
    }

    /**
     * @brief Checks whether the value is a string.
     *
     * @return True if the value is a string, false otherwise.
     */
    bool is_string() const
    {
      return std::holds_alternative<std::string>(v);
    }

    /**
     * @brief Checks whether the value is an integer.
     *
     * @return True if the value is an integer, false otherwise.
     */
    bool is_int() const
    {
      return std::holds_alternative<long long>(v);
    }

    /**
     * @brief Checks whether the value is a double.
     *
     * @return True if the value is a double, false otherwise.
     */
    bool is_double() const
    {
      return std::holds_alternative<double>(v);
    }

    /**
     * @brief Checks whether the value is a boolean.
     *
     * @return True if the value is a boolean, false otherwise.
     */
    bool is_bool() const
    {
      return std::holds_alternative<bool>(v);
    }

    /**
     * @brief Returns the stored string value.
     *
     * The caller must ensure that is_string() is true before calling this.
     *
     * @return Stored string value.
     */
    const std::string &as_string() const
    {
      return std::get<std::string>(v);
    }

    /**
     * @brief Returns the stored integer value.
     *
     * The caller must ensure that is_int() is true before calling this.
     *
     * @return Stored integer value.
     */
    long long as_int() const
    {
      return std::get<long long>(v);
    }

    /**
     * @brief Returns the stored double value.
     *
     * The caller must ensure that is_double() is true before calling this.
     *
     * @return Stored double value.
     */
    double as_double() const
    {
      return std::get<double>(v);
    }

    /**
     * @brief Returns the stored boolean value.
     *
     * The caller must ensure that is_bool() is true before calling this.
     *
     * @return Stored boolean value.
     */
    bool as_bool() const
    {
      return std::get<bool>(v);
    }
  };

  /**
   * @brief Represents a parsed function-style REPL call.
   *
   * Supported examples:
   * - `println("hi")`
   * - `Vix.cd("..")`
   */
  struct CallExpr
  {
    /**
     * @brief Object name for member calls.
     *
     * Empty for global calls.
     */
    std::string object;

    /**
     * @brief Member name for object calls.
     */
    std::string member;

    /**
     * @brief Function name for global calls.
     */
    std::string callee;

    /**
     * @brief Parsed argument values.
     */
    std::vector<CallValue> args;

    /**
     * @brief Raw argument text.
     *
     * This is useful when an argument must later be resolved as a variable,
     * path expression, or arithmetic expression.
     */
    std::vector<std::string> args_raw;
  };

  /**
   * @brief Result returned by the REPL call parser.
   */
  struct CallParseResult
  {
    /**
     * @brief True if parsing succeeded.
     */
    bool ok = false;

    /**
     * @brief Parsed call expression.
     */
    CallExpr expr;

    /**
     * @brief Error message when parsing fails.
     */
    std::string error;
  };

  /**
   * @brief Parses a function-style REPL call.
   *
   * Supported forms:
   * - `name(arg1, arg2, ...)`
   * - `Obj.name(arg1, ...)`
   *
   * Supported literals:
   * - `"string"` and `'string'`
   * - `123` and `12.34`
   * - `true`, `false`, and `null`
   *
   * @param input Input text to parse.
   * @return Parsed call result.
   */
  CallParseResult parse_call(std::string_view input);

  /**
   * @brief Checks whether input looks like a function-style call.
   *
   * This is a lightweight detection helper. It does not fully validate the
   * input. Use parse_call() for full parsing.
   *
   * @param input Input text to inspect.
   * @return True if the input looks like a call, false otherwise.
   */
  bool looks_like_call(std::string_view input);
}

#endif
