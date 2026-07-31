#pragma once
#include <gdkmm/paintable.h>
#include <gtkmm/icontheme.h>

#include <mutex>
#include <string>

namespace waybar::util {

class DefaultGtkIconThemeWrapper {
 private:
  static std::mutex default_theme_mutex;

 public:
  static bool has_icon(const std::string&);
  static Glib::RefPtr<Gdk::Paintable> load_icon(
      const char* name, int size, Gtk::IconLookupFlags flags = Gtk::IconLookupFlags::FORCE_REGULAR);
};

}  // namespace waybar::util
