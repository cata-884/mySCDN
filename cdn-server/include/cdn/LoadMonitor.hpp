#pragma once

#include "cdn/NodeConfig.hpp"
#include <cstddef>
#include <memory>
#include <mutex>
class loadMonitor : public std::enable_shared_from_this<loadMonitor> {
private:
  // todo: trece la atomic in loc de mutex
  mutable std::mutex m_mutex;
  std::size_t conexiuniMaxime;
  std::size_t clientiConectati = 0;

public:
  // analog cu un bilet cand intri intr-o parcare cu plata
  class ticket {
  private:
    // vedem daca s-a putut conecta clientul
    // nullptr => nu; some(x) => da
    // nu e in niciun caz unique ptr, pentru ca ar distruge intreg loadMonitor
    // atunci cand clientul se deconecteaza
    std::shared_ptr<loadMonitor> monitorPtr;

  public:
    explicit ticket(std::shared_ptr<loadMonitor> ptr) noexcept;
    // fara copii
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