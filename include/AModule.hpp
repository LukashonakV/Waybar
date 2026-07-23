#pragma once

#include <fmt/args.h>
#include <fmt/format.h>
#include <glibmm/dispatcher.h>
#include <glibmm/markup.h>
#include <gtkmm.h>
#include <json/json.h>

#include <string>
#include <utility>

#include "IModule.hpp"

namespace waybar {

class AModule : public IModule {
 public:
  static constexpr const char* MODULE_CLASS = "module";

  ~AModule() override;
  // vilu1214  sigc::signal<void(AModule*)> signal_updated;
  auto doUpdate() -> void override;
  virtual auto doRefresh(int shouldRefresh) -> void {};
  operator Gtk::Widget&() override final;
  auto doAction(const std::string& name) -> void override;

  /// Emitting on this dispatcher triggers a update() call
  Glib::Dispatcher dp;

  bool expandEnabled() const;

  virtual void suspend() {};
  virtual void resume() {};
  bool shouldSuspend() const { return disable_on_sleep_; }

 protected:
  // Don't need to make an object directly
  // Derived classes are able to use it
  AModule(const Json::Value&, const std::string&, const std::string&, bool enable_click = false,
          bool enable_scroll = false);

  const std::string name_;
  const Json::Value& config_;
  std::vector<int> pid_;
  Glib::RefPtr<Gtk::GestureClick> controllClick_;
  Glib::RefPtr<Gtk::EventControllerMotion> controllMotion_;
  bool disable_on_sleep_{false};

  virtual Gtk::Widget& getWidget() = 0;
  virtual void handleToggle(int n_press, double x, double y);
  bool tooltipEnabled() const;
  void bindEvents(Gtk::Widget& wg);
  void unBindEvents();

  // --- Generic format/tooltip resolution (config-only, usable by any module,
  // ALabel-derived or not). Prefers `<key>-<state>`, then `<key>`, then default.
  std::string resolveFormat(const std::string& defaultFormat, const std::string& state = "") const {
    if (!state.empty() && config_["format-" + state].isString()) {
      return config_["format-" + state].asString();
    }
    if (config_["format"].isString()) {
      return config_["format"].asString();
    }
    return defaultFormat;
  }
  std::string resolveTooltipFormat(const std::string& defaultFormat,
                                   const std::string& state = "") const {
    if (!state.empty() && config_["tooltip-format-" + state].isString()) {
      return config_["tooltip-format-" + state].asString();
    }
    if (config_["tooltip-format"].isString()) {
      return config_["tooltip-format"].asString();
    }
    return defaultFormat;
  }

  // Generic tooltip for any widget: honors the `tooltip` toggle and
  // `tooltip-format`, formats with the given args and applies it. Lets modules
  // that are not ALabel-derived (e.g. gamemode) reuse the shared logic.
  template <typename... Args>
  void updateTooltip(Gtk::Widget& widget, const std::string& defaultFormat, Args&&... args) {
    if (!tooltipEnabled()) {
      return;
    }
    widget.set_tooltip_markup(fmt::format(fmt::runtime(resolveTooltipFormat(defaultFormat)),
                                          std::forward<Args>(args)...));
  }

  // Maps a configured event name (e.g. "on-click-middle") to a built-in module
  // action name. Populated from the `actions` config section, and by modules
  // that interpret on-click* config values as internal actions (e.g.
  // wlr/taskbar). Entries here are dispatched through doAction() instead of
  // being run as shell commands.
  std::map<std::string, std::string> eventActionMap_;

 private:
  const bool isAfter{true};
  bool enableClick_{false};
  bool enableScroll_{false};
  bool hasPressEvents_{false};
  bool hasReleaseEvents_{false};

  Glib::RefPtr<Gtk::EventControllerScroll> controllScroll_;
  const bool isTooltip_;
  const bool isExpand_;
  bool hasUserEvents_;
  gdouble distance_scrolled_y_;
  gdouble distance_scrolled_x_;
  const Glib::RefPtr<Gdk::Cursor> curDefault_;
  const Glib::RefPtr<Gdk::Cursor> curPoint_;
  Glib::RefPtr<const Gdk::Event> currEvent_;
  sigc::connection cursor_timeout_conn_;
  static const inline std::map<std::tuple<uint, int, Gdk::Event::Type>, std::string> eventMap_{
      {std::make_tuple(1u, 1, Gdk::Event::Type::BUTTON_PRESS), "on-click"},
      {std::make_tuple(1u, 1, Gdk::Event::Type::BUTTON_RELEASE), "on-click-release"},
      {std::make_tuple(1u, 2, Gdk::Event::Type::BUTTON_PRESS), "on-double-click"},
      {std::make_tuple(1u, 3, Gdk::Event::Type::BUTTON_PRESS), "on-triple-click"},
      {std::make_tuple(2u, 1, Gdk::Event::Type::BUTTON_PRESS), "on-click-middle"},
      {std::make_tuple(2u, 1, Gdk::Event::Type::BUTTON_RELEASE), "on-click-middle-release"},
      {std::make_tuple(2u, 2, Gdk::Event::Type::BUTTON_PRESS), "on-double-click-middle"},
      {std::make_tuple(2u, 3, Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-middle"},
      {std::make_tuple(3u, 1, Gdk::Event::Type::BUTTON_PRESS), "on-click-right"},
      {std::make_tuple(3u, 1, Gdk::Event::Type::BUTTON_RELEASE), "on-click-right-release"},
      {std::make_tuple(3u, 2, Gdk::Event::Type::BUTTON_PRESS), "on-double-click-right"},
      {std::make_tuple(3u, 3, Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-right"},
      {std::make_tuple(8u, 1, Gdk::Event::Type::BUTTON_PRESS), "on-click-backward"},
      {std::make_tuple(8u, 1, Gdk::Event::Type::BUTTON_RELEASE), "on-click-backward-release"},
      {std::make_tuple(8u, 2, Gdk::Event::Type::BUTTON_PRESS), "on-double-click-backward"},
      {std::make_tuple(8u, 3, Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-backward"},
      {std::make_tuple(9u, 1, Gdk::Event::Type::BUTTON_PRESS), "on-click-forward"},
      {std::make_tuple(9u, 1, Gdk::Event::Type::BUTTON_RELEASE), "on-click-forward-release"},
      {std::make_tuple(9u, 2, Gdk::Event::Type::BUTTON_PRESS), "on-double-click-forward"},
      {std::make_tuple(9u, 3, Gdk::Event::Type::BUTTON_PRESS), "on-triple-click-forward"},
      {std::make_tuple(10u, 1, Gdk::Event::Type::BUTTON_PRESS), "on-click-copy"}};
  enum SCROLL_DIR { NONE, UP, DOWN, LEFT, RIGHT };

  const SCROLL_DIR getScrollDir(Glib::RefPtr<const Gdk::Event> e);

  void handleClickEvent(uint n_button, int n_press, double x, double y, Gdk::Event::Type n_evtype);
  void handleRelease(int n_press, double x, double y);
  void handleMouseEnter(double x, double y);
  void handleMouseLeave();
  bool handleScroll(double dx, double dy);
  void makeControllClick();
  void makeControllScroll();
  void makeControllMotion();
  void removeControllClick();
  void removeControllScroll();
  void removeControllMotion();
  void setCursor(const Glib::ustring& name);
  // Backward-compat overload for legacy numeric Gdk::CursorType configs (pre-0.16)
  void setCursor(const Glib::RefPtr<Gdk::Cursor>& cur);
};

}  // namespace waybar
