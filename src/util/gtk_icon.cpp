#include "util/gtk_icon.hpp"

namespace waybar::util {

std::mutex DefaultGtkIconThemeWrapper::default_theme_mutex;

Glib::RefPtr<Gtk::IconTheme> DefaultGtkIconThemeWrapper::get_theme() {
  const std::lock_guard<std::mutex> lock(default_theme_mutex);
  return Gtk::IconTheme::get_for_display(Gdk::Display::get_default());
}

bool DefaultGtkIconThemeWrapper::has_icon(const std::string& value) {
  return get_theme()->has_icon(value);
}

Glib::RefPtr<Gdk::Paintable> DefaultGtkIconThemeWrapper::load_icon(const char* name, int size,
                                                                   Gtk::IconLookupFlags flags) {
  // lookup_icon in GTK4 returns an IconPaintable (which is a Paintable).
  // Symbolic colors are applied automatically when the widget renders it.
  return get_theme()->lookup_icon(name, size, 1, Gtk::TextDirection::NONE, flags);
}

}  // namespace waybar::util
