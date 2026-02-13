#include "cdn/LoadMonitor.hpp"
#include "cdn/NodeConfig.hpp"

loadMonitor::loadMonitor(const NodeConfig &config) noexcept
    : conexiuniMaxime(config.maxConexiuni) {}

loadMonitor::ticket loadMonitor::tryAquire() {
  std::size_t current = clientiConectati.load(std::memory_order_relaxed);
  while (current < conexiuniMaxime) {
    if (clientiConectati.compare_exchange_weak(current, current + 1,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
      return ticket(shared_from_this());
    }
  }
  return ticket(nullptr);
}

void loadMonitor::Release() noexcept {
  clientiConectati.fetch_sub(1, std::memory_order_acq_rel);
}

std::size_t loadMonitor::ActiveConnections() const noexcept {
  return clientiConectati.load(std::memory_order_relaxed);
}

loadMonitor::ticket::ticket(std::shared_ptr<loadMonitor> ptr) noexcept
    : monitorPtr(std::move(ptr)) {}

loadMonitor::ticket::ticket(ticket &&other) noexcept
    : monitorPtr(std::move(other.monitorPtr)) {}

loadMonitor::ticket &loadMonitor::ticket::operator=(ticket &&other) noexcept {
  if (this != &other) {

    if (monitorPtr) {
      monitorPtr->Release();
    }

    monitorPtr = std::move(other.monitorPtr);
  }
  return *this;
}
loadMonitor::ticket::~ticket() {

  if (monitorPtr) {
    monitorPtr->Release();
  }
}