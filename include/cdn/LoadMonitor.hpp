#pragma once

#include <cstddef>
#include <mutex> 
#include<memory>

class loadMonitor : public std::enable_shared_from_this<loadMonitor>{
private:
    //todo: trece la atomic in loc de mutex
    std::size_t conexiuniMaxime; 
    std::size_t clientiConectati = 0; 
    mutable std::mutex m_mutex; 
public:
    //analog cu un bilet cand intri intr-o parcare cu plata
    class ticket {
    private:
        //vedem daca s-a putut conecta clientul
        //nullptr => nu; some(x) => da
        std::shared_ptr<loadMonitor> monitorPtr;
    public:
        explicit ticket(std::shared_ptr<loadMonitor> ptr) noexcept;
        //fara copii
        ticket(const ticket&) = delete;
        ticket& operator=(const ticket&) = delete;

        ticket(ticket&& other) noexcept;
        ticket& operator=(ticket&& other) noexcept;
        
        ~ticket();

        bool Valid() const noexcept {
            return monitorPtr != nullptr;
        }
    };

    explicit loadMonitor(std::size_t maxConexiuni) noexcept;

    ticket tryAquire();
    void Release() noexcept;

    std::size_t MaxConnections() const noexcept {
        return conexiuniMaxime;
    }

    std::size_t ActiveConnections() const noexcept;   
};