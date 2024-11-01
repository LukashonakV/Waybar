#pragma once

#include <giomm/dbusproxy.h>
#include <gtkmm/box.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>

#include "ALabel.hpp"

namespace waybar::modules {

class Gamemode final : public ALabel {
 public:
  Gamemode(const std::string &, const Json::Value &);
  virtual ~Gamemode();
  auto update() -> void override;

 private:
  const std::string DEFAULT_ICON_NAME = "input-gaming-symbolic";
  const std::string DEFAULT_FORMAT = "{glyph}";
  const std::string DEFAULT_FORMAT_ALT = "{glyph} {count}";
  const std::string DEFAULT_TOOLTIP_FORMAT = "Games running: {count}";
  const std::string DEFAULT_GLYPH = "";

  void appear(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name,
              const Glib::ustring &name_owner);
  void disappear(const Glib::RefPtr<Gio::DBus::Connection> &connection, const Glib::ustring &name);
  void prepareForSleep_cb(const Glib::RefPtr<Gio::DBus::Connection> &connection,
                          const Glib::ustring &sender_name, const Glib::ustring &object_path,
                          const Glib::ustring &interface_name, const Glib::ustring &signal_name,
                          const Glib::VariantContainerBase &parameters);
  void notify_cb(const Glib::ustring &sender_name, const Glib::ustring &signal_name,
                 const Glib::VariantContainerBase &arguments);

  void getData();
  void handleToggle(int n_press, double dx, double dy) override;

  // Config
  std::string format = DEFAULT_FORMAT;
  std::string format_alt = DEFAULT_FORMAT_ALT;
  std::string tooltip_format = DEFAULT_TOOLTIP_FORMAT;
  std::string glyph = DEFAULT_GLYPH;
  bool tooltip = true;
  bool hideNotRunning = true;
  bool useIcon = true;
  uint iconSize = 20;
  uint iconSpacing = 4;
  std::string iconName = DEFAULT_ICON_NAME;

  Gtk::Box box_;
  Gtk::Image icon_;
  Gtk::Label label_;

  const std::string dbus_name = "com.feralinteractive.GameMode";
  const std::string dbus_obj_path = "/com/feralinteractive/GameMode";
  const std::string dbus_interface = "org.freedesktop.DBus.Properties";
  const std::string dbus_get_interface = "com.feralinteractive.GameMode";

  uint gameCount = 0;

  std::string lastStatus;
  bool showAltText = false;

  guint login1_id;
  Glib::RefPtr<Gio::DBus::Proxy> gamemode_proxy;
  Glib::RefPtr<Gio::DBus::Connection> system_connection;
  Glib::RefPtr<Gtk::IconTheme> gtkTheme_;
  bool gamemodeRunning;
  guint gamemodeWatcher_id;
};

}  // namespace waybar::modules
