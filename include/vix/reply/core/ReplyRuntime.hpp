/**
 *  @file ReplyRuntime.hpp
 *  @brief Non-interactive Reply runtime for embedded execution.
 *
 *  Vix Reply
 */

#ifndef VIX_REPLY_CORE_REPLY_RUNTIME_HPP
#define VIX_REPLY_CORE_REPLY_RUNTIME_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vix::reply
{
  struct ReplyRuntimeOptions
  {
    bool enableCalculator = true;
    bool allowProcessExit = false;
    bool allowWorkingDirectoryChange = false;
    bool allowFilesystemWrite = false;
    bool allowHistory = false;
  };

  struct ReplyRunResult
  {
    bool ok = true;

    bool exitRequested = false;
    int exitCode = 0;

    std::string stdoutText;
    std::string stderrText;
    std::string valueText;
    std::string errorText;

    long long elapsedMs = 0;
  };

  class ReplyRuntime
  {
  public:
    explicit ReplyRuntime(ReplyRuntimeOptions options = {});
    ~ReplyRuntime();

    ReplyRuntime(const ReplyRuntime &) = delete;
    ReplyRuntime &operator=(const ReplyRuntime &) = delete;

    ReplyRuntime(ReplyRuntime &&) noexcept;
    ReplyRuntime &operator=(ReplyRuntime &&) noexcept;

    ReplyRunResult run_line(std::string_view line);
    ReplyRunResult run_cell(std::string_view source);

    void set_args(std::vector<std::string> args);
    void clear();

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
  };
}

#endif
