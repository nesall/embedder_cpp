#ifndef _HTTPSERVER_H_
#define _HTTPSERVER_H_

#include <memory>

class App;

class HttpServer {
  struct Impl;
  std::unique_ptr<Impl> imp;

public:
  HttpServer(App &app);
  ~HttpServer();
  bool startServer(int port);
  void stop();

private:
  //void startWatch(int intervalSeconds);
  //void stopWatch();
};

#endif // _HTTPSERVER_H_