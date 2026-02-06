#include "cdn/NodeConfig.hpp"
#include "utils/ErrorHandling.hpp"
#include <string_view>

std::pair<std::string, std::uint16_t> processIP_Port(const std::string& input) {
    const auto it = input.rfind(':');
    throwIF(it == std::string::npos,
            "Format invalid 'IP:Port'. Lipseste ':' in: " + input);

    std::string ip = input.substr(0, it);
    const std::string portStr = input.substr(it + 1);

    const int port = std::stoi(portStr);
    throwIF(port < 0 || port > 65535, "Port invalid (range 0-65535): " + portStr);

    return {ip, static_cast<std::uint16_t>(port)};
}

PeerDescriptor processPeer(const std::string& input) {
    // Format: nodeID@IP:Port
    const auto it = input.find('@');
    throwIF(it == std::string::npos,
            "Format peer invalid. Asteptat 'id@ip:port', primit: " + input);

    std::string id = input.substr(0, it);
    const std::string address = input.substr(it + 1);

    const auto [ip, port] = processIP_Port(address);
    return {id, ip, port};
}

const PeerDescriptor* NodeConfig::findNode(const std::string& id) const {
    for (const auto& peer : m_peers) {
        if (peer.ID == id) {
            return &peer;
        }
    }
    return nullptr;
}

const PeerDescriptor& NodeConfig::self() const {
    const PeerDescriptor* ptr = findNode(m_node_id);
    throwIF(!ptr, "CRITIC: Nodul curent (" + m_node_id + ") nu se regaseste in configuratia clusterului!");
    return *ptr;
}
/*
./myscdn_node \
    --node-id node1 \
    --listen 10.100.0.30:8000 \
    --advertise-addr 192.168.1.50:8000 \  # IP-ul vizibil de clienti (sau domeniu)
    --cluster-node node2@10.100.0.30:8001 \
    --target-files /mnt/storage_hdd/movies \
    --temp-path /mnt/fast_ssd/transcode_buffer \ # Pt remuxing rapid
    --max-connections 4096 \              # 10 e prea putin pt io_uring!
    --cache-bytes 2147483648 \            # 2GB RAM Cache (100MB e putin pt video)
    --workers 4 \                         # Foloseste primele 4 nuclee exclusiv
    --direct-io \                         # Bypass OS Cache (citire directa disc->net)
    --ttl 3600 \
    --db-path ../CDN.db \
    --auth-file ../auth.txt
 */
NodeConfig ParseArguments(const int argc, char* argv[]) {
    NodeConfig config;

    bool idSet = false;
    bool listenSet = false;
    bool targetSet = false;
    bool dbSet = false;
    for (int i = 1; i < argc; ++i) {
        if (const std::string_view arg = argv[i]; arg == "--node-id" && i + 1 < argc) {
            config.m_id = argv[++i];
            idSet = true;
        }
        else if (arg == "--listen" && i + 1 < argc) {
            const auto [ip, port] = processIP_Port(argv[++i]);
            config.ipAddress = ip;
            config.m_port = port;
            listenSet = true;
        }
        else if (arg == "--cluster-node" && i + 1 < argc) {
            config.m_peers.push_back(processPeer(argv[++i]));
        }
        else if ((arg == "--target-files" || arg == "--origin-root") && i + 1 < argc) {
            config.m_target = argv[++i];
            targetSet = true;
        }
        else if (arg == "--max-connections" && i + 1 < argc) {
            config.m_max_connections = std::stoul(argv[++i]);
        }
        else if (arg == "--db-path" && i + 1 < argc) {
            config.m_db = argv[++i];
            dbSet = true;
        }
        else if (arg == "--cache-bytes" && i + 1 < argc) {
            config.m_cache_capacity = std::stoul(argv[++i]);
        }
        else if (arg == "--ttl" && i + 1 < argc) {
            config.ttlImplicit = std::chrono::seconds(std::stoll(argv[++i]));
        }
    }

    throwIF(!idSet, "Argument obligatoriu lipsa: --node-id");
    throwIF(!listenSet, "Argument obligatoriu lipsa: --listen");
    // Daca nu seteaza target files, poti lasa default "../targetFiles" sau poti forta eroare:
    throwIF(!targetSet, "Argument obligatoriu lipsa: --target-files");
    throwIF(!dbSet, "Argument obligatoriu lipsa: --db-path");

    bool selfPresent = false;
    for (const auto& peer : config.m_peers) {
        if (peer.ID == config.m_node_id) {
            selfPresent = true;
            // Optional: Verificam daca IP-ul declarat in --cluster-node coincide cu --listen
            // Daca difera, e posibil sa fie o eroare de configurare, dar nu dam throw.
            break;
        }
    }

    if (!selfPresent) {
        config.m_peers.emplace_back(config.m_node_id, config.ipAddress, config.m_port);
    }

    return config;
}