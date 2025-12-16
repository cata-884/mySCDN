#include "cdn/HashRing.hpp"
#include "cdn/Types.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include <cstddef>
#include<algorithm>

std::size_t HasheazaResursa(const std::string& text) {
    // std::hash oferă o distribuție mult mai bună decât algoritmul liniar
    return std::hash<std::string>{}(text);
}

hashRing::hashRing(std::vector<PeerDescriptor> noduri) : serviceNodes(std::move(noduri)) {
    throwIF(serviceNodes.empty(), "Hashring necesita macar un nod");
    //sortam dupa hashID
    std::sort(serviceNodes.begin(), serviceNodes.end(), 
        [](const PeerDescriptor& a, const PeerDescriptor& b) {
            return HasheazaResursa(a.ID) < HasheazaResursa(b.ID);
        }
    );
}

PeerDescriptor hashRing::Locate(const std::string& resursa) const{
    std::lock_guard<std::mutex> lock(threadMutex);
    std::size_t valoareHash = HasheazaResursa(resursa);
    //cautam primul nod care are Hash-ul >= HashResursa
    //se foloseste o functie lambda
    auto it = std::lower_bound(serviceNodes.begin(), serviceNodes.end(), valoareHash,
        [](const PeerDescriptor& nod, std::size_t valoareHash) {
            return HasheazaResursa(nod.ID) < valoareHash;
        }
    );
    //wrap around: daca nu s-a gasit elementul(it == serviceNodes.end()), inseamna ca hash-ul resurse este mai mare decat hash-ul ultimului nod. trecem deci la primul nod.
    if (it == serviceNodes.end()) {
        return serviceNodes.front();
    }
    return *it;
}

PeerDescriptor hashRing::NextAfter(const std::string& idNod) const{
    std::lock_guard<std::mutex> lock(threadMutex);
    if(serviceNodes.size()<=1){
        return PeerDescriptor{};
    }
    for(std::size_t i = 0; i<serviceNodes.size(); i++){
        if(serviceNodes[i].ID == idNod){
            return serviceNodes[(i+1)%serviceNodes.size()];
        }
    }
    return serviceNodes[0]; //fallback
}
/*
Acum, dacă ai nodurile A (Hash 100), B (Hash 500), C (Hash 900):

    Resursa cu Hash 350 -> Merge la B (primul > 350).

    Resursa cu Hash 950 -> Nu e nimeni > 950 -> Wrap around -> Merge la A.
*/

//dupa ce adaugam nodul, sortam dupa hash
void hashRing::AddNode(const PeerDescriptor& node) {
    std::lock_guard<std::mutex> lock(threadMutex);
    serviceNodes.push_back(node);
    std::sort(serviceNodes.begin(), serviceNodes.end(), [](const PeerDescriptor& a, const PeerDescriptor& b) {
        return HasheazaResursa(a.ID) < HasheazaResursa(b.ID);
    });
}
std::vector<PeerDescriptor> hashRing::Nodes() const {
    std::lock_guard<std::mutex> lock(threadMutex); 
    return serviceNodes; 
}
    
