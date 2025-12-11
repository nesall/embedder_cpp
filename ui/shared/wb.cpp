#include "wb.h"
#include "utils.h"
#include <filesystem>
#include <utils_log/logger.hpp>

#if defined(__linux__)
#include <gtk/gtk.h>
#endif


#ifdef _WIN32
#define IDI_ICON1 101
#endif

namespace fs = std::filesystem;

namespace {
#ifdef WIN32
  static LRESULT CALLBACK BeforeDestroySubclass(
    HWND hwnd, UINT msg, WPARAM w, LPARAM l,
    UINT_PTR id, DWORD_PTR refData)
  {
    auto *self = reinterpret_cast<Webview *>(refData);
    if (msg == WM_CLOSE) {
      if (self->onDestroyCallback_) {
        self->onDestroyCallback_();
      }
    }
    return DefSubclassProc(hwnd, msg, w, l);
  }
#elif defined(__APPLE__)
  // TODO:
#elif defined(__linux__)
  static gboolean OnDeleteEventThunk(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    auto *self = static_cast<Webview *>(user_data);
    if (self->onDestroyCallback_) self->onDestroyCallback_();
    return FALSE;   // allow default handler to continue (destroy window)
  }
#endif
} // anonymous namespace


Webview::Webview(bool debug, void *pWindow) : webview::webview(debug, pWindow)
{
#ifdef WIN32
  HWND hWnd = static_cast<HWND>(window().value());
  SetWindowSubclass(hWnd, BeforeDestroySubclass, 1, reinterpret_cast<DWORD_PTR>(this));
#elif defined(__linux__)
  GtkWidget *w = (GtkWidget *)window().value();
  g_signal_connect(
    G_OBJECT(w),
    "delete-event",     // fires before destroy
    G_CALLBACK(&OnDeleteEventThunk),
    this
  );
#endif
}

void Webview::setAppIcon(const std::string &iconBaseName)
{
  auto assets = findWebAssets();
  if (assets.empty()) return;
  std::filesystem::path base = std::filesystem::path(assets) / iconBaseName;
  std::string iconPath = (base.parent_path() / (base.stem().string() + ".png")).string();
  setAppIconImpl(window().value(), iconPath);
}

#ifndef __APPLE__
void Webview::setAppIconImpl(void *wnd, const std::string &iconPath)
{
#if defined(_WIN32)
  HICON hIconSmall = (HICON)LoadImage(
    GetModuleHandle(nullptr),
    MAKEINTRESOURCE(IDI_ICON1),
    IMAGE_ICON,
    16, 16, 0);

  HICON hIconBig = (HICON)LoadImage(
    GetModuleHandle(nullptr),
    MAKEINTRESOURCE(IDI_ICON1),
    IMAGE_ICON,
    32, 32, 0);

  HWND hwnd = (HWND)window().value();
  SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
  SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
#elif defined(__linux__)
  GError *error = nullptr;
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(iconPath.c_str(), &error);
  if (pixbuf) {
    gtk_window_set_icon(GTK_WINDOW(window().value()), pixbuf);
    g_object_unref(pixbuf);
  }
#endif
}

std::pair<int, int> Webview::getWindowSize()
{
  std::pair<int, int> res;
#ifdef WIN32
  HWND hwnd = (HWND)window().value();
  RECT rect;
  GetClientRect(hwnd, &rect);
  res.first = rect.right - rect.left;
  res.second = rect.bottom - rect.top;
#elif defined(__linux__)
  GtkWindow *gtkWindow = GTK_WINDOW(window().value());
  gint width = 0;
  gint height = 0;
  gtk_window_get_size(gtkWindow, &width, &height);
  res.first = width;
  res.second = height;
#endif
  return res;
}
#endif // __APPLE__

// static
std::string Webview::findWebAssets()
{
  LOG_START;
  std::string exeDir = shared::getExecutableDir();
  std::vector<std::string> paths = {
      exeDir + "/web_assets",
      exeDir + "/../web_assets",
      "web_assets",
      "../web_assets",
      "../../spa-svelte/dist"
  };
  for (const auto &path : paths) {
    if (fs::exists(path) && fs::exists(fs::path(path) / "index.html")) {
      return path;
    }
  }
  return "";
}
