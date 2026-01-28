#include "cdn/NodeConfig.hpp"
#include "cdn/Types.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include <string>
#include <vector>

std::pair<std::string, std::uint16_t> processIP_Port(const std::string &input) {
  const auto it = input.find(':');
  throwIF(it == std::string::npos,
          "Input-ul de forma '127.0.0.1:8080' nu poate fi procesat, lipseste "
          "caracterului ':'");
  std::string ipAdress = input.substr(0, it);
  const std::string port = input.substr(it + 1);
  return std::make_pair(ipAdress, static_cast<std::uint16_t>(std::stoi(port)));
}

PeerDescriptor processPeer(const std::string &input) {
  const auto it = input.find('@');
  throwIF(it == std::string::npos,
          "Input-ul de de forma 'node1@127.0.0.1:8080' nu poate fi procesat, "
          "lipseste caracterul '@'");
  std::string node = input.substr(0, it);
  const std::string IP_Port = input.substr(it + 1);
  const auto [fst, snd] = processIP_Port(IP_Port);
  return {node, fst, snd};
}

const PeerDescriptor *NodeConfig::findNode(const std::string &id) const {
  for (const auto &peer : peersVector) {
    if (peer.ID == id) {
      return &peer;
    }
  }
  return nullptr;
}

const PeerDescriptor &NodeConfig::self() const {
  const PeerDescriptor *ptr = findNode(nodeId);
  throwIF(!ptr, "Nodul curent (" + nodeId +
                 ") nu a fost gasit in lista de cluster-nodes!");
  return *ptr;
}
/*
init cluster 2 noduri
./myscdn_node \
    --node-id node1 \
    --listen 10.100.0.30:8000 \
    --cluster-node node2@10.100.0.30:8001 \
    --target-files ../targetFiles \
    --max-connections 10 \
    --cache-bytes 104857600 \
    --ttl 3600 \
    --db-path ../CDN.db \
    --auth-file ../auth.txt
*/
NodeConfig ParseArguments(const int argc, char *argv[]) {
  NodeConfig config;
  bool idSet = false;
  bool listenSet = false;
  bool targetSet = false;
  bool maxConnSet = false;
  bool cacheSet = false;
  bool ttlSet = false;
  bool dbSet = false;

  for (int i = 1; i < argc; ++i) {
    // numele noului nod declarat
    if (std::string arg = argv[i]; arg == "--node-id" && i + 1 < argc) {
      config.nodeId = argv[++i];
      idSet = true;
    }
    // detalii pentru a rula serverul si a asculta comenzi
    else if (arg == "--listen" && i + 1 < argc) {
      const auto [fst, snd] = processIP_Port(argv[++i]);
      config.ipAddress = fst;
      config.port = snd;
      listenSet = true;
    }
    // detaliile subnodurilor colege
    else if (arg == "--cluster-node" && i + 1 < argc) {
      config.peersVector.push_back(processPeer(argv[++i]));
      // clusterSet = true;
    }
    // de unde preluam datele
    else if (arg == "--target-files" && i + 1 < argc) {
      config.targetFilesLocation = argv[++i];
      targetSet = true;
    }
    // in rest e self-explanatory
    else if (arg == "--max-connections" && i + 1 < argc) {
      config.maxConexiuni = std::stoul(argv[++i]);
      maxConnSet = true;
    } else if (arg == "--db-path" && i + 1 < argc) {
      config.dbPath = std::string(argv[++i]);
      dbSet = true;
    } else if (arg == "--cache-bytes" && i + 1 < argc) {
      config.cacheSize = std::stoul(argv[++i]);
      cacheSet = true;
    } else if (arg == "--ttl" && i + 1 < argc) {
      long long sec = std::stoll(argv[++i]);
      config.ttlImplicit = std::chrono::seconds(sec);
      ttlSet = true;
    }
  }

  throwIF(!idSet, "Argument lipsa: --node-id este obligatoriu");

  throwIF(!listenSet, "Argument lipsa: --listen este obligatoriu");

  throwIF(
      !targetSet,
      "Argument lipsa: --target-files (sau --origin-root) este obligatoriu");

  throwIF(!maxConnSet, "Argument lipsa: --max-connections este obligatoriu");

  throwIF(!cacheSet, "Argument lipsa: --cache-bytes este obligatoriu");

  throwIF(!ttlSet, "Argument lipsa: --ttl este obligatoriu");

  throwIF(!dbSet, "Argument lipsa: --db-path este obligatoriu");

  // ne asiguram ca nodul specificat in cl face parte din peersVector. nu putem
  // sa avem Node1 <-> Node2 cand doar Node2 face parte din
  // vector<PeerDescriptor>
  bool selfFound = false;
  for (const auto &p : config.peersVector) {
    if (p.ID == config.nodeId) {
      selfFound = true;
      break;
    }
  }

  if (!selfFound) {
    config.peersVector.emplace_back(config.nodeId, config.ipAddress, config.port);
  }

  return config;
}
