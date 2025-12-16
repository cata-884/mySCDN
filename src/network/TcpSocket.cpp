#include "network/TcpSocket.hpp"
#include "miscellaneous/ErrorHandling.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>

int openSocket(){
    int sockFD = ::socket(AF_INET, SOCK_STREAM, 0);
    throwIF(sockFD<0, "S-a esuat crearea unui socket");
    int opt = 1;
    int sockOptResult = ::setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(sockOptResult < 0){
        ::close(sockFD);
        throwIF(true, "S-a esuat refolosirea aceluias IP");
    }
    return sockFD;
}
TcpSocket::TcpSocket() : sockFD(openSocket()) {}
TcpSocket::TcpSocket(int fd) : sockFD(fd) {}
TcpSocket::~TcpSocket() { Close(); }

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : sockFD(other.sockFD) {
    other.sockFD = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if(this != &other){
        Close();
        sockFD = other.sockFD;
        other.sockFD = -1;
    }
    return *this;
}

void TcpSocket::Bind(std::uint16_t port, const std::string& ipAdress){
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port= htons(port);
    if(ipAdress.empty()){
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    else {
        int ipSetStatus = ::inet_pton(AF_INET, ipAdress.c_str(), &addr.sin_addr);
        throwIF(ipSetStatus <= 0, ("Adresa IP invalida:" + ipAdress).c_str());
    }
    int bind_status = ::bind(sockFD, /*(sockaddr*)&addr*/ reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    throwIF(bind_status < 0, ("Eroare la bind folosind IP-ul " + 
                                                    (ipAdress.empty() ? "ANY" : ipAdress) + 
                                                    " si portul " + std::to_string(port)).c_str());
}

void TcpSocket::Listen(int backlog) const{
    throwIF(::listen(sockFD, backlog) < 0, "Eroare la Listen");
}

TcpSocket TcpSocket::Accept() const{
    int clientFD = ::accept(sockFD, nullptr, nullptr);
    throwIF(clientFD < 0, "Eroare la Accept");
    return TcpSocket(clientFD);
}

void TcpSocket::Close(){
    if(sockFD >= 0){
        ::close(sockFD);
        sockFD = -1;
    }
}

void TcpSocket::Connect(std::uint16_t port, const std::string& ipAdress) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, ipAdress.c_str(), &addr.sin_addr) <= 0) {
        throwIF(true, ("Adresa IP invalida la connect: " + ipAdress).c_str());
    }

    int res = ::connect(sockFD, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    throwIF(res < 0, "Nu s-a putut realiza conexiunea la server");
}

void TcpSocket::SendAll(const std::string& mesaj) const {
    SendAll(mesaj.data(), mesaj.size());
}

void TcpSocket::SendAll(const void* mesaj, std::size_t len) const {
    const char* buff = static_cast<const char*>(mesaj);
    std::size_t total = 0;
    //trimitem pe chunks
    while(total < len){
        std::size_t trimis = ::send(sockFD, buff + total, len-total, 0);
        throwIF(trimis<=0, "Eroare la send");
        total += trimis;
    }
}

std::string TcpSocket::recvLine(std::size_t len) const{
    std::string linie;
    linie.reserve(64);
    while(linie.size() < len){
        char c;
        ssize_t primit = ::recv(sockFD, &c, 1, 0);
        throwIF(primit <= 0, "Conexiunea s-a incheiat in timp ce se citea linia");
        if(c == '\n'){
            break;
        }
        //windows-ul transmite si \r\n
        if(c != '\r'){
            linie.push_back(c);
        }
    }
    return linie;
}

std::string TcpSocket::recvN(std::size_t len) const{
    std::string data(len, 0);
    std::size_t totalPrimit = 0;
    while(totalPrimit < len){
        ssize_t partialPrimit = ::recv(sockFD, &data[0]+totalPrimit, len - totalPrimit, 0);
        throwIF(partialPrimit <= 0, "Eroare la citire continutului din fisier(payload)");
        totalPrimit+=static_cast<std::size_t>(partialPrimit);
    }
    return data;
}
bool TcpSocket::isValid() const {
    return sockFD>=0;
}

std::size_t TcpSocket::Recv(void* buffer, std::size_t len) const {
    ssize_t primit = ::recv(sockFD, buffer, len, 0);
    return primit;
}
