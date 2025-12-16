#include "cdn/NodeServer.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include<memory>
#include<mutex>

void directoryExists(const std::string& path){
    struct stat st{};
    if (stat(path.c_str(), &st) == -1) {
        mkdir(path.c_str(), 0700);
    }
}

NodeServer::NodeServer(NodeConfig config)
    : m_config(std::move(config)),
      m_cache(m_config.cacheSize, m_config.ttlImplicit),
      m_ring(m_config.peersVector),
      m_monitor(std::make_shared<loadMonitor>(m_config.maxConexiuni))
{
    directoryExists(m_config.targetFilesLocation);
    if (m_config.nodeId != "node1") { 
        JoinCluster();
    }
}

std::unique_ptr<std::string> NodeServer::FetchFromOrigin(const std::string& resursa) {
    std::string sep = "/";
    if(!m_config.targetFilesLocation.empty() && m_config.targetFilesLocation.back() == '/') sep = "";
    std::string filePath = m_config.targetFilesLocation + sep + resursa;
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) return nullptr;

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::unique_ptr<std::string> fileContent(new std::string());
    fileContent->resize(fileSize);
    if(file.read(&(*fileContent)[0], fileSize)){
        return fileContent;
    }
    return nullptr;
}

std::unique_ptr<std::string> NodeServer::FetchFromPeer(const PeerDescriptor& peer, const std::string& resursa) {
    try {
        TcpSocket socket;
        socket.Connect(peer.port, const_cast<std::string&>(peer.ipAdress)); 
        
        std::string cerere = "GET /" + resursa + "\n";
        socket.SendAll(cerere);
        
        std::string header = socket.recvLine();
        std::istringstream iss(header);
        std::string proto, status;
        iss >> proto >> status;
        
        if (status == "OK") {
            std::size_t fileSize;
            iss >> fileSize;
            std::string data = socket.recvN(fileSize);
            return std::unique_ptr<std::string>(new std::string(std::move(data)));
        }
    } catch (const std::exception& ex) {
        Log("Peer fetch failed from " + peer.ID + ": " + ex.what());
    }
    return nullptr;
}

void NodeServer::ResolveContent(const std::string& resursa, TcpSocket& client) {
    auto cacheLocal = m_cache.Get(resursa);
    if (cacheLocal) {
        Log("Cache HIT: " + resursa);
        std::string header = "RESP OK " + std::to_string(cacheLocal->size()) + "\n";
        client.SendAll(header);
        client.SendAll(*cacheLocal);
        return;
    }
    Log("Cache MISS: " + resursa);

    auto proprietar = m_ring.Locate(resursa);

    if (proprietar.ID != m_config.nodeId) {
        Log("Fetching from Peer: " + proprietar.ID + " (" + proprietar.ipAdress + ":" + std::to_string(proprietar.port) + ")");
        
        try {
            TcpSocket peerSocket;
            peerSocket.Connect(proprietar.port, proprietar.ipAdress);

            std::string req = "GET " + resursa + "\n";
            peerSocket.SendAll(req);

            std::string header = peerSocket.recvLine();
            
            if (header.find("ERROR") != std::string::npos) {
                Log("Peer returned error: " + header);
                client.SendAll(header);
                return;
            }

            client.SendAll(header + "\n"); 

            char buffer[8192];
            std::string ramBuffer; 
            
            while (true) {
                ssize_t bytesRead = peerSocket.Recv(buffer, sizeof(buffer));
                if (bytesRead <= 0) break; 

                client.SendAll(buffer, static_cast<std::size_t>(bytesRead));
            }

            peerSocket.Close();
            Log("Peer streaming complet (Relay).");

        } catch (const std::exception& e) {
            Log("Eroare la Peer Fetch: " + std::string(e.what()));
            try { client.SendAll("RESP ERROR Peer Fetch Failed\n"); } catch(...) {}
        }
    } 
    else {
        Log("Fetching from Origin (Disk) & STREAMING...");
        
        std::string sep = "/";
        if(!m_config.targetFilesLocation.empty() && m_config.targetFilesLocation.back() == '/') sep = "";
        std::string originPath = m_config.targetFilesLocation + sep + resursa;

        std::ifstream sourceFile(originPath, std::ios::binary);

        if (!sourceFile.is_open()) {
            Log("Eroare: Fisierul nu exista pe disc: " + originPath);
            client.SendAll("RESP ERROR Not Found\n");
            return;
        }

        client.SendAll("RESP OK STREAMING\n");

        char buffer[8192];
        std::string ramBuffer;

        while (sourceFile.read(buffer, sizeof(buffer)) || sourceFile.gcount() > 0) {
            std::streamsize bytesRead = sourceFile.gcount();
            
            try {
                client.SendAll(buffer, static_cast<std::size_t>(bytesRead));
            } catch (...) {
                Log("Client disconnected during streaming.");
                break;
            }

            ramBuffer.append(buffer, bytesRead);
        }

        sourceFile.close();

        if (!ramBuffer.empty()) {
            m_cache.Put(resursa, ramBuffer);
        }

        Log("Streaming Disk->Client complet.");
    }
}

void NodeServer::Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    std::cout << "Nodul " << m_config.nodeId << " - " << message << std::endl;
}

void NodeServer::HandleConnection(TcpSocket client) {
    auto ticket = m_monitor->tryAquire();

    if (!ticket.Valid()) {
        try { client.SendAll("RESP ERROR Server Overloaded\n"); } catch (...) {}
        return; 
    }

    try {
        std::string requestLine = client.recvLine();
        if (requestLine.empty()) return;

        Log("Request: " + requestLine);
        std::istringstream iss(requestLine);
        std::string command;
        iss >> command;

        if (command == "GET") {
            std::string resource;
            iss >> resource;
            if (!resource.empty() && resource[0] == '/') resource.erase(0, 1);
            ResolveContent(resource, client);
        }
        else if (command == "JOIN") {
            std::string newNodeID, newNodeIP;
            int newNodePort;
            iss >> newNodeID >> newNodeIP >> newNodePort;

            Log("CLUSTER UPDATE: Nod nou detectat -> " + newNodeID);
            
            PeerDescriptor newPeer{newNodeID, newNodeIP, (uint16_t)newNodePort};
            m_ring.AddNode(newPeer); 
            
            client.SendAll("RESP OK WELCOME\n");
        }
        else {
            client.SendAll("RESP ERROR Comanda necunoscuta\n");
        }

    } catch (const std::exception& e) {
        Log("Eroare conexiune: " + std::string(e.what()));
    }
}
void NodeServer::JoinCluster() {
    if (m_config.peersVector.empty()) {
        Log("Sunt Seed Node (Master). Astept conexiuni...");
        return;
    }

    const auto& seedNode = m_config.peersVector[0]; 

    Log("Incerc sa ma alatur clusterului prin: " + seedNode.ID);

    try {
        TcpSocket socket;
        std::string ip = seedNode.ipAdress; 
        socket.Connect(seedNode.port, ip);

        std::string joinCmd = "JOIN " + m_config.nodeId + " " + 
                              m_config.ipAddress + " " + 
                              std::to_string(m_config.port) + "\n";
        
        socket.SendAll(joinCmd);

        std::string response = socket.recvLine();
        if (response.find("OK WELCOME") != std::string::npos) {
            Log("SUCCESS: M-am alaturat clusterului!");
        } else {
            Log("WARNING: Seed-ul nu a confirmat JOIN-ul.");
        }

        socket.Close();

    } catch (std::exception& e) {
        Log("CRITICAL: Nu m-am putut conecta la Seed Node! Voi lucra izolat. Eroare: " + std::string(e.what()));
    }
}