#pragma once

#include <memory>
#include <string>

class AdminAuth {
public:
  AdminAuth();
  ~AdminAuth();

  // parameter is {'password', 'Bearer|Basic'}
  bool authenticate(const std::pair<std::string, std::string> &pass, std::string &jwtToken);
  bool isDefaultPassword();
  void setPassword(const std::string &newPassword);

  std::string fileLastModifiedTime() const;

  static std::string fnv1a64(const std::string &str);

private:
  struct Impl;
  std::unique_ptr<Impl> imp;
};
