/**
 *  @file Vix.hpp
 *  @brief Small runtime object exposed inside the Vix Reply REPL.
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

#ifndef VIX_REPLY_API_VIX_HPP
#define VIX_REPLY_API_VIX_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vix::reply
{
  class History;
}

namespace vix::reply::api
{
  /**
   * @brief Result returned by Vix Reply runtime operations.
   */
  struct VixResult
  {
    /**
     * @brief Operation status code.
     *
     * A value of `0` usually means success.
     */
    int code = 0;

    /**
     * @brief Optional human-readable message.
     *
     * This is usually filled when an operation fails.
     */
    std::string message;
  };

  /**
   * @brief Runtime helper object exposed to the Reply REPL.
   *
   * The Vix object provides a small, safe API available from the REPL,
   * such as current directory access, environment variable reading,
   * argument inspection, history operations, and controlled exit.
   */
  class Vix
  {
  public:
    /**
     * @brief Creates a Vix runtime helper.
     *
     * @param history Pointer to the active REPL history storage.
     */
    explicit Vix(vix::reply::History *history);

    /**
     * @brief Returns the current process id.
     *
     * @return Process id.
     */
    int pid() const;

    /**
     * @brief Requests the REPL to exit.
     *
     * @param code Exit code to return from the REPL.
     */
    void exit(int code = 0);

    /**
     * @brief Checks whether an exit was requested.
     *
     * @return True if the REPL should exit, false otherwise.
     */
    bool exit_requested() const;

    /**
     * @brief Returns the requested exit code.
     *
     * @return Exit code.
     */
    int exit_code() const;

    /**
     * @brief Returns the current working directory.
     *
     * @return Current working directory path.
     */
    std::filesystem::path cwd() const;

    /**
     * @brief Changes the current working directory.
     *
     * @param path Target directory path.
     * @return Operation result.
     */
    VixResult cd(const std::string &path);

    /**
     * @brief Creates a directory.
     *
     * @param path      Directory path to create.
     * @param recursive True to create parent directories as needed.
     * @return Operation result.
     */
    VixResult mkdir(const std::string &path, bool recursive = true);

    /**
     * @brief Reads an environment variable.
     *
     * @param key Environment variable name.
     * @return Environment variable value, or std::nullopt when not found.
     */
    std::optional<std::string> env(const std::string &key) const;

    /**
     * @brief Returns the arguments passed to the REPL.
     *
     * @return Read-only reference to the stored arguments.
     */
    const std::vector<std::string> &args() const;

    /**
     * @brief Stores the arguments passed to the REPL.
     *
     * @param args Arguments to store.
     */
    void set_args(std::vector<std::string> args);

    /**
     * @brief Prints the REPL history.
     *
     * @return Operation result.
     */
    VixResult history();

    /**
     * @brief Clears the REPL history.
     *
     * @return Operation result.
     */
    VixResult history_clear();

  private:
    /**
     * @brief Active REPL history storage.
     */
    vix::reply::History *history_ = nullptr;

    /**
     * @brief True when the REPL has been requested to exit.
     */
    bool exitRequested_ = false;

    /**
     * @brief Exit code returned when the REPL exits.
     */
    int exitCode_ = 0;

    /**
     * @brief Arguments passed to the REPL.
     */
    std::vector<std::string> args_;
  };
}

#endif
