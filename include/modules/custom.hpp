#pragma once

#include "ALabel.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class Custom final : public ALabel {
 public:
  Custom(const std::string&, const std::string&, const Json::Value&, const std::string&);
  virtual ~Custom();
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void continuousWorker();
  void waitingWorker();
  void parseOutputRaw();
  void parseOutputJson();
  void handleEvent();
  bool handleScroll(double dx, double dy);
  void handleToggle(int n_press, double dx, double dy);

  const std::string name_;
  const std::string output_name_;
  std::string text_;
  std::string id_;
  std::string alt_;
  std::string tooltip_;
  const bool tooltip_format_enabled_;
  std::vector<std::string> class_;
  int percentage_;
  FILE* fp_;
  int pid_;
  util::command::res output_;
  util::JsonParser parser_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
