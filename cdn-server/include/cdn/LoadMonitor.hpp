#pragma once

#include "cdn/NodeConfig.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
class loadMonitor : public std::enable_shared_from_this<loadMonitor> {
private:
  std::size_t conexiuniMaxime;
  std::atomic<std::size_t> clientiConectati{0};

public:
  class ticket {
  private:
    std::shared_ptr<loadMonitor> monitorPtr;

  public:
    explicit ticket(std::shared_ptr<loadMonitor> ptr) noexcept;

    ticket(const ticket &) = delete;
    ticket &operator=(const ticket &) = delete;

    ticket(ticket &&other) noexcept;
    ticket &operator=(ticket &&other) noexcept;

    ~ticket();

    [[nodiscard]] bool Valid() const noexcept { return monitorPtr != nullptr; }
  };

  explicit loadMonitor(const NodeConfig &config) noexcept;

  ticket tryAquire();
  void Release() noexcept;

  std::size_t MaxConnections() const noexcept { return conexiuniMaxime; }

  std::size_t ActiveConnections() const noexcept;
};