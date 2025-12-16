#include "cdn/NodeConfig.hpp"
#include "cdn/Types.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include <string>
#include <vector>
#include<memory>
#include<mutex>

std::pair<std::string, std::uint16_t> processIP_Port(const std::string& input){
    auto it = input.find(':');
    throwIF(it == std::string::npos, "Input-ul de forma '127.0.0.1:8080' nu poate fi procesat, lipseste caracterului ':'");
    std::string ipAdress = input.substr(0, it);
    std::string port = input.substr(it+1);
    return std::make_pair(ipAdress, static_cast<std::uint16_t>(std::stoi(port)));
}

PeerDescriptor processPeer(const std::string& input){
    auto it = input.find('@');
    throwIF(it == std::string::npos, "Input-ul de de forma 'node1@127.0.0.1:8080' nu poate fi procesat, lipseste caracterul '@'");
    std::string node = input.substr(0, it);
    std::string IP_Port = input.substr(it+1);
    std::pair<std::string, std::uint16_t> ipInfo = processIP_Port(IP_Port);
    return PeerDescriptor(node, ipInfo.first, ipInfo.second);
}

const PeerDescriptor* NodeConfig::findNode(const std::string& id) const {
    for (const auto& peer : peersVector) {
        if (peer.ID == id) {
            return &peer;
        }
    }
    return nullptr;
}

const PeerDescriptor& NodeConfig::self() const {
    const PeerDescriptor* ptr = findNode(nodeId);
    throwIF(!ptr, ("Nodul curent (" + nodeId + ") nu a fost gasit in lista de cluster-nodes!").c_str());
    return *ptr;
}
//example input : ./myscdn_node --node-id node1 --listen 127.0.0.1:8080 --cluster-node node2@127.0.0.1:9000 --cluster-node node3@127.0.0.1:5000 --max-connections 5
NodeConfig ParseArguments(int argc, char* argv[]) {
    NodeConfig config;
    bool idSet = false;
    bool listenSet = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        //numele noului nod declarat
        if (arg == "--node-id" && i + 1 < argc) {
            config.nodeId = argv[++i];
            idSet = true;
        } 
        //detalii pentru a rula serverul si a asculta comenzi
        else if (arg == "--listen" && i + 1 < argc) {
            std::pair<std::string, std::uint16_t> ipInfo = processIP_Port(argv[++i]);
            config.ipAddress = ipInfo.first;
            config.port = ipInfo.second;
            listenSet = true;
        } 
        //detaliile subnodurilor colege
        else if (arg == "--cluster-node" && i + 1 < argc) {
            config.peersVector.push_back(processPeer(argv[++i]));
        } 
        //de unde preluam datele
        else if ((arg == "--target-files" || arg == "--origin-root") && i + 1 < argc) {
            config.targetFilesLocation = argv[++i];
        }
        //in rest e self explanatory 
        else if (arg == "--max-connections" && i + 1 < argc) {
            config.maxConexiuni = std::stoul(argv[++i]);
        }
        else if (arg == "--cache-bytes" && i + 1 < argc) {
            config.cacheSize = std::stoul(argv[++i]);
        } 
        else if (arg == "--ttl" && i + 1 < argc) {
            long long sec = std::stoll(argv[++i]);
            config.ttlImplicit = std::chrono::seconds(sec);
        }
    }

    throwIF(!idSet, "Argument lipsa: --node-id este obligatoriu");
    throwIF(!listenSet, "Argument lipsa: --listen este obligatoriu");

    bool selfFound = false;
    for(const auto& p : config.peersVector) {
        if(p.ID == config.nodeId) {
            selfFound = true;
            break;
        }
    }
    
    if (!selfFound) {
        config.peersVector.push_back(PeerDescriptor(config.nodeId, config.ipAddress, config.port));
    }

    return config;
}
