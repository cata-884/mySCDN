#include "network/TcpSocket.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include<cstdlib>
//daca avem ceva de forma 'targetFiles/music.mp3' taiem la 'music.mp3'
std::string GetFileName(const std::string& path) {
    size_t it = path.find_last_of("/\\");
    if (it == std::string::npos) return path;
    return path.substr(it + 1);
}

//functia simpla de redare - only for linux
//todo: adaptare windows + mac?
void Play(const std::string& filepath) {
    std::string command = "xdg-open " + filepath + " > /dev/null 2>&1";
    
    std::system(command.c_str());
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Comanda are forma: ./myscdn_client <IP> <PORT>" << std::endl;
        return 1;
    }
    std::string ip = argv[1];
    std::uint16_t port = static_cast<std::uint16_t>(std::stoi(argv[2]));

    try {
        TcpSocket socket;
        socket.Connect(port, ip);
        std::cout << "Client conectat la " << ip << ":" << port << std::endl;

        std::cout << "Alege o melodie prezenta in targetFiles (e.g., /nightcall.mp3): ";
        std::string resource;
        std::getline(std::cin, resource);   
        //trimitem mesaj
        socket.SendAll("GET " + resource + "\n");

        //primim raspuns de la server
        std::string header = socket.recvLine();
        std::istringstream iss(header);
        std::string proto, status;
        iss >> proto >> status;

        if (status == "OK") {
            std::size_t fileSize;
            iss >> fileSize;

            std::cout << "Au fost primiti " << fileSize << " bytes..." << std::endl;
            //payload response
            std::string fileData = socket.recvN(fileSize);

            std::string tempPath = "/tmp/cdn_" + GetFileName(resource);
            
            std::ofstream outFile(tempPath.c_str(), std::ios::binary);
            if (outFile.is_open()) {
                outFile.write(fileData.data(), fileData.size());
                outFile.close();
                Play(tempPath);

            } else {
                std::cerr << "Eroare : nu s-a putut salva fisierul temporar" << std::endl;
            }

        } else {
            std::cout << "Eroare de server" << header << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Eroare : " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
