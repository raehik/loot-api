/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

    This file is part of LOOT.

    LOOT is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    LOOT is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with LOOT.  If not, see
    <https://www.gnu.org/licenses/>.
    */
#ifndef LOOT_API_HELPERS_LOGGING
#define LOOT_API_HELPERS_LOGGING

#include <spdlog/spdlog.h>

#include "loot/enum/log_level.h"

namespace loot {
static const char* LOGGER_NAME = "loot_api_logger";

inline std::shared_ptr<spdlog::logger> getLogger() {
  return spdlog::get(LOGGER_NAME);
}

class SpdLoggingSink : public spdlog::sinks::base_sink<std::mutex> {
public:
  SpdLoggingSink(std::function<void(LogLevel, const char*)> callback) {
    this->callback = callback;
  }

protected:
  void _sink_it(const spdlog::details::log_msg& msg) override {
    callback(mapFromSpdlog(msg.level), msg.raw.str().c_str());
  }

  void _flush() override {}

private:
  std::function<void(LogLevel, const char*)> callback;

  static LogLevel mapFromSpdlog(spdlog::level::level_enum severity) {
    using spdlog::level::level_enum;
    switch (severity) {
      case level_enum::trace:
        return LogLevel::trace;
      case level_enum::debug:
        return LogLevel::debug;
      case level_enum::info:
        return LogLevel::info;
      case level_enum::warn:
        return LogLevel::warning;
      case level_enum::err:
        return LogLevel::error;
      case level_enum::critical:
        return LogLevel::fatal;
      default:
        return LogLevel::trace;
    }
  }
};
}

#endif
