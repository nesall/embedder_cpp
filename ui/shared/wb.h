#include <webview/webview.h>
#include <functional>

class Webview : public webview::webview {
public:
  std::function<void()> onDestroyCallback_;
public:
  Webview(bool debug, void *pWindow);

  void on_window_created() override {
    webview::webview::on_window_created();
  }

  void on_window_destroyed(bool skip_termination) override {
    //if (onDestroyCallback_) onDestroyCallback_();
    webview::webview::on_window_destroyed(skip_termination);
  }

  void setAppIcon(const std::string assetsBase, const std::string &iconBaseName);
  std::pair<int, int> getWindowSize();
  static std::string findWebAssets(const std::string &base);

private:
  void setAppIconImpl(void *wnd, const std::string &iconPath);
};