#include "cdn/HashRing.hpp"
#include "cdn/NodeConfig.hpp"
#include "cdn/Types.hpp"
#include "utils/ErrorHandling.hpp"
#include <algorithm>
#include <cstddef>
//"ceva.mp3" => Σ (int)c
// daca Serverul A și Serverul B calculează hash-ul pentru "video.mp4", ambele
// vor obtine exact acelasi numar. Acest lucru este critic pentru ca toate
// nodurile să fie de acord asupra locației fișierului

std::size_t HasheazaResursa(const std::string &text) {
  std::size_t sum = 0;
  for (const char c : text) {
    sum += static_cast<unsigned char>(c);
  }
  return sum;
}

// avem nevoie de o hashare determinista pentru a nu avea situatii de tipul:
// server A(node1) - (node1, node2, node3, ...)
// server B(node2) - (node3, node1, node2, ...)
hashRing::hashRing(const NodeConfig &config)
    : serviceNodes(config.peersVector) {
  throwIF(serviceNodes.empty(), "Hashring necesita macar un nod");
  // sortam alfabetic pentru a asigura structura node1 => node2 => ...
  std::ranges::sort(serviceNodes,
                    [](const PeerDescriptor &a, const PeerDescriptor &b) {
                      return a.ID < b.ID;
                    });
}

// folosit pentru identificarea carui nod este responsabil, DACA nodul este
// online. daca e offline, se recalculeaza distributia
PeerDescriptor hashRing::Locate(const std::string &resursa) const {
  std::lock_guard lock(threadMutex);
  const std::size_t valoareHash = HasheazaResursa(resursa);
  const std::size_t index = valoareHash % serviceNodes.size();
  return serviceNodes[index];
}
// vecinul idNod-ului
PeerDescriptor hashRing::NextAfter(const std::string &idNod) const {
  std::lock_guard lock(threadMutex);
  if (serviceNodes.size() <= 1)
    return PeerDescriptor{};
  for (std::size_t i = 0; i < serviceNodes.size(); i++) {
    if (serviceNodes[i].ID == idNod) {
      return serviceNodes[(i + 1) % serviceNodes.size()];
    }
  }
  return serviceNodes[0];
}

void hashRing::AddNode(const PeerDescriptor &node) {
  std::lock_guard lock(threadMutex);
  for (const auto &n : serviceNodes) {
    if (n.ID == node.ID)
      return;
  }
  // introducem noul nod si resortam alfabetic
  serviceNodes.push_back(node);
  std::ranges::sort(serviceNodes,
                    [](const PeerDescriptor &a, const PeerDescriptor &b) {
                      return a.ID < b.ID;
                    });
}

std::vector<PeerDescriptor> hashRing::Nodes() const {
  std::lock_guard lock(threadMutex);
  return serviceNodes;
}