#include "util/gtk_icon.hpp"

namespace waybar::util {

std::mutex DefaultGtkIconThemeWrapper::default_theme_mutex;

bool DefaultGtkIconThemeWrapper::has_icon(const std::string& value) {
  const std::lock_guard<std::mutex> lock(default_theme_mutex);
  auto theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
  return theme->has_icon(value);
}

Glib::RefPtr<Gdk::Paintable> DefaultGtkIconThemeWrapper::load_icon(const char* name, int size,
                                                                   Gtk::IconLookupFlags flags) {
  const std::lock_guard<std::mutex> lock(default_theme_mutex);

  auto theme = Gtk::IconTheme::get_for_display(Gdk::Display::get_default());

  // lookup_icon in GTK4 returns an IconPaintable (which is a Paintable).
  // Symbolic colors are applied automatically when the widget renders it.
  return theme->lookup_icon(name, size, 1, Gtk::TextDirection::NONE, flags);
}

}  // namespace waybar::util
