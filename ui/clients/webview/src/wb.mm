#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import "wb.h"

void Webview::setAppIconImpl(void *wnd, const std::string &iconPath)
{
  (void)wnd;
  NSString *path = [NSString stringWithUTF8String:iconPath.c_str()];
  NSImage *icon = [[NSImage alloc]initWithContentsOfFile:path];
  if (icon)[NSApp setApplicationIconImage:icon];
}

std::pair<int, int> Webview::getWindowSize()
{
  std::pair<int, int> res;
  NSWindow *nsWindow = (__bridge NSWindow *)window().value();
  NSRect rect = [nsWindow contentRectForFrameRect:[nsWindow frame] ];
  res.first = rect.size.width;
  res.second = rect.size.height;
  return res;
}


#endif // __APPLE__
