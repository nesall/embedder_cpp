#pragma once

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class InstanceRegistry {
public:
  explicit InstanceRegistry(const std::string &registryPath = {});
  ~InstanceRegistry();

  // non-copyable, but movable
  InstanceRegistry(const InstanceRegistry &) = delete;
  InstanceRegistry &operator=(const InstanceRegistry &) = delete;
  InstanceRegistry(InstanceRegistry &&) = default;
  InstanceRegistry &operator=(InstanceRegistry &&) = default;

  void registerInstance(int port, const std::string &name = "");
  void unregister();
  void startHeartbeat();
  void stopHeartbeat();
  std::vector<json> getActiveInstances() const;
  std::string getInstanceId() const;

private:
  struct Impl;
  std::unique_ptr<Impl> imp;
};