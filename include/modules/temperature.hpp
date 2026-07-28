#pragma once

#include <fmt/format.h>

#include <fstream>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Temperature final : public ALabel {
 public:
  Temperature(const std::string&, const Json::Value&);
  virtual ~Temperature() = default;
  auto doUpdate() -> void override;
  void suspend() override;
  void resume() override;

 private:
  float getTemperature();
  bool isCritical(uint16_t);
  bool isWarning(uint16_t);

  std::string file_path_;
  util::SleeperThread thread_;
};

}  // namespace waybar::modules
