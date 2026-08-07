#pragma once

#include <giomm/desktopappinfo.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>

#include <string>
#include <vector>

namespace waybar::util {

class IconLoader {
 private:
  std::vector<Glib::RefPtr<Gtk::IconTheme>> custom_icon_themes_;
  static std::vector<std::string> search_prefix();
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_by_name(const std::string& app_id);
  static Glib::RefPtr<Gio::DesktopAppInfo> get_desktop_app_info(const std::string& app_id);
  static Glib::RefPtr<Gtk::IconPaintable> load_icon_from_file(const std::string& icon_path,
                                                              int size);
  static std::string get_icon_name_from_icon_theme(const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                                   const std::string& app_id);
  static bool image_load_icon(Gtk::Image& image, const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                              Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size);

 public:
  void add_custom_icon_theme(const std::string& theme_name);
  bool image_load_icon(Gtk::Image& image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                       int size) const;
  static Glib::RefPtr<Gio::DesktopAppInfo> get_app_info_from_app_id_list(
      const std::string& app_id_list);
};

}  // namespace waybar::util
