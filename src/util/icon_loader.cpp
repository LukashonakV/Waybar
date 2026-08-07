#include "util/icon_loader.hpp"

#include <spdlog/spdlog.h>

#include "util/gtk_icon.hpp"
#include "util/string.hpp"

namespace waybar::util {

std::vector<std::string> IconLoader::search_prefix() {
  std::vector<std::string> prefixes = {""};

  const char* home_env = std::getenv("HOME");
  std::string home_dir = home_env ? home_env : "";
  if (!home_dir.empty()) {
    prefixes.push_back(home_dir + "/.local/share/");
  }

  auto xdg_data_dirs = std::getenv("XDG_DATA_DIRS");
  if (!xdg_data_dirs) {
    prefixes.emplace_back("/usr/share/");
    prefixes.emplace_back("/usr/local/share/");
  } else {
    std::string xdg_data_dirs_str(xdg_data_dirs);
    size_t start = 0;
    size_t end = 0;

    do {
      end = xdg_data_dirs_str.find(':', start);
      auto p = xdg_data_dirs_str.substr(start, end - start);
      prefixes.push_back(trim(p) + "/");

      start = end == std::string::npos ? end : end + 1;
    } while (end != std::string::npos);
  }

  for (auto& p : prefixes) spdlog::debug("Using 'desktop' search path prefix: {}", p);

  return prefixes;
}

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_app_info_by_name(const std::string& app_id) {
  static std::vector<std::string> prefixes = search_prefix();

  std::vector<std::string> app_folders = {"", "applications/", "applications/kde/",
                                          "applications/org.kde."};

  std::vector<std::string> suffixes = {"", ".desktop"};

  for (auto const& prefix : prefixes) {
    for (auto const& folder : app_folders) {
      for (auto const& suffix : suffixes) {
        auto app_info_ =
            Gio::DesktopAppInfo::create_from_filename(prefix + folder + app_id + suffix);
        if (!app_info_) {
          continue;
        }

        return app_info_;
      }
    }
  }

  return {};
}

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_desktop_app_info(const std::string& app_id) {
  if (auto app_info = get_app_info_by_name(app_id)) {
    return app_info;
  }

  std::string desktop_file;
  const auto all_apps = Gio::AppInfo::get_all();

  for (const auto& app_info : all_apps) {
    const auto id = app_info->get_id();
    if (id.empty()) {
      continue;
    }

    if (desktop_file.empty()) {
      desktop_file = id;
    }

    auto tmp_info = Gio::DesktopAppInfo::create(id);
    if (!tmp_info) {
      continue;
    }

    if (tmp_info->get_startup_wm_class() == app_id) {
      desktop_file = id;
      break;
    }
  }

  return get_app_info_by_name(desktop_file);
}

Glib::RefPtr<Gtk::IconPaintable> IconLoader::load_icon_from_file(const std::string& icon_path,
                                                                 int size) {
  return Gtk::IconPaintable::create(Gio::File::create_for_path(icon_path), size, size);
}

bool IconLoader::image_load_icon(Gtk::Image& image, const Glib::RefPtr<Gtk::IconTheme>& icon_theme,
                                 Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size) {
  const auto icon{app_info->get_icon()};
  const auto paintable{icon_theme->lookup_icon(icon, size, image.get_scale_factor(),
                                               image.get_direction(), Gtk::IconLookupFlags::NONE)};

  if (paintable) {
    image.property_paintable().set_value(paintable);
    return true;
  }

  return false;
}

void IconLoader::add_custom_icon_theme(const std::string& theme_name) {
  auto icon_theme = Gtk::IconTheme::create();
  icon_theme->set_theme_name(theme_name);
  custom_icon_themes_.push_back(icon_theme);
  spdlog::debug("Use custom icon theme: {}", theme_name);
}

bool IconLoader::image_load_icon(Gtk::Image& image, Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                                 int size) const {
  for (auto& icon_theme : custom_icon_themes_) {
    if (image_load_icon(image, icon_theme, app_info, size)) {
      return true;
    }
  }

  return image_load_icon(image, DefaultGtkIconThemeWrapper::get_theme(), app_info, size);
}

Glib::RefPtr<Gio::DesktopAppInfo> IconLoader::get_app_info_from_app_id_list(
    const std::string& app_id) {
  Glib::RefPtr<Gio::DesktopAppInfo> app_info_{get_desktop_app_info(app_id)};
  if (app_info_) {
    return app_info_;
  }

  auto lower_app_id = app_id;
  std::ranges::transform(lower_app_id, lower_app_id.begin(),
                         [](char c) { return std::tolower(c); });
  app_info_ = get_desktop_app_info(lower_app_id);
  if (app_info_) {
    return app_info_;
  }

  size_t start = 0, end = app_id.size();
  start = app_id.rfind(".", end);
  std::string app_name = app_id.substr(start + 1, app_id.size());
  app_info_ = get_desktop_app_info(app_name);
  if (app_info_) {
    return app_info_;
  }

  start = app_id.find("-");
  app_name = app_id.substr(0, start);
  app_info_ = get_desktop_app_info(app_name);

  return app_info_;
}

}  // namespace waybar::util
