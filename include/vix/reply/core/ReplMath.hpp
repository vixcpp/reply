/**
 *  @file ReplMath.hpp
 *  @brief Arithmetic expression evaluator for the Vix Reply REPL.
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

#ifndef VIX_REPLY_CORE_REPL_MATH_HPP
#define VIX_REPLY_CORE_REPL_MATH_HPP

#include <optional>
#include <string>

namespace vix::reply
{
  /**
   * @brief Result produced by the REPL arithmetic evaluator.
   */
  struct CalcResult
  {
    /**
     * @brief Numeric result of the evaluated expression.
     */
    double value = 0.0;

    /**
     * @brief Human-readable formatted result.
     */
    std::string formatted;
  };

  /**
   * @brief Evaluates a basic arithmetic expression.
   *
   * Supported syntax:
   * - binary operators: `+`, `-`, `*`, `/`, `%`
   * - unary operators: `+`, `-`
   * - parentheses
   * - integer and floating-point numbers
   *
   * @param expr  Arithmetic expression to evaluate.
   * @param error Output string receiving the error message on failure.
   * @return Calculation result on success, `std::nullopt` on failure.
   */
  std::optional<CalcResult> eval_expression(const std::string &expr, std::string &error);
}

#endif
