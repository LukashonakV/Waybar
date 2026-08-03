#pragma once

#include <gtkmm/widget.h>

namespace waybar {

class IModule {
 public:
  virtual ~IModule() = default;
  virtual auto doUpdate() -> void = 0;
  virtual auto doAction(const std::string& name) -> void = 0;
  virtual operator Gtk::Widget&() = 0;
};

}  // namespace waybar
