#include "cdn/LoadMonitor.hpp"

loadMonitor::loadMonitor(std::size_t maxConexiuni) noexcept
    : conexiuniMaxime(maxConexiuni), clientiConectati(0) {}

loadMonitor::ticket loadMonitor::tryAquire() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (conexiuniMaxime == 0 || clientiConectati < conexiuniMaxime) {
        if (conexiuniMaxime > 0) {
            clientiConectati++;
        }
        //shared_from_this() ne permite sa avem o instanta de shared pointer atunci cand tot ce avem e this
        return ticket(shared_from_this());
    }
    //failure
    return ticket(nullptr);
}

void loadMonitor::Release() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (conexiuniMaxime > 0 && clientiConectati > 0) {
        clientiConectati--;
    }
}

std::size_t loadMonitor::ActiveConnections() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return clientiConectati;
}

loadMonitor::ticket::ticket(std::shared_ptr<loadMonitor> ptr) noexcept
    : monitorPtr(std::move(ptr)) {}

loadMonitor::ticket::ticket(ticket&& other) noexcept
    : monitorPtr(std::move(other.monitorPtr)) {
    //shared_ptr devine automat null dupa move, nu trebuie setat manual
}

loadMonitor::ticket& loadMonitor::ticket::operator=(ticket&& other) noexcept {
    if (this != &other) {
        //daca aveam un loc ocupat, il eliberam
        if (monitorPtr) {
            monitorPtr->Release();
        }
        //furam pointerul
        monitorPtr = std::move(other.monitorPtr);
    }
    return *this;
}
loadMonitor::ticket::~ticket() {
    //daca pointerul e valid, inseamna ca am avut loc => facem Release
    if (monitorPtr) {
        monitorPtr->Release();
    }
}