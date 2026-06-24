/**
 *
 *  @file reply.hpp
 *  @author Gaspard Kirira
 *
 *  @brief Public entry point for the Vix reply module.
 *
 *  Provides a clean and stable API for the Vix interactive reply and REPL
 *  subsystem, including API dispatching, call parsing, console handling,
 *  line editing, history, math helpers, flow control, and REPL utilities.
 *
 *  Usage:
 *    #include <vix/reply/reply.hpp>
 *
 *  For advanced usage, include specific components from:
 *    <vix/reply/...>
 *
 *  Copyright 2026, Gaspard Kirira.
 *  All rights reserved.
 *  https://github.com/vixcpp/vix
 *
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 */
#ifndef VIX_REPLY_REPLY_HPP
#define VIX_REPLY_REPLY_HPP

#include <vix/reply/api/ReplApi.hpp>
#include <vix/reply/api/ReplCallParser.hpp>
#include <vix/reply/api/Vix.hpp>

#include <vix/reply/console/ReplConsole.hpp>

#include <vix/reply/core/ReplDetail.hpp>
#include <vix/reply/core/ReplDispatcher.hpp>
#include <vix/reply/core/ReplFlow.hpp>
#include <vix/reply/core/ReplHistory.hpp>
#include <vix/reply/core/ReplLineEditor.hpp>
#include <vix/reply/core/ReplMath.hpp>
#include <vix/reply/core/ReplUtils.hpp>

#include <vix/reply/core/ReplyRuntime.hpp>

#endif // VIX_REPLY_REPLY_HPP
