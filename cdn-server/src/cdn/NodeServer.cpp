#include "cdn/NodeServer.hpp"
#include "cdn/Database.hpp"
#include "cdn/LoadMonitor.hpp"
#include "cdn/Types.hpp"
#include <cstddef>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <vector>

static constexpr std::size_t limita_superioara = 50 * 1024 * 1024;

// access_logs = {id, node_id, file_name, client_ip, timestamp}

static bool e_cale_sigura(const std::string &nume) {
  if (nume.empty())
    return false;
  if (nume[0] == '/')
    return false;
  if (nume.find("..") != std::string::npos)
    return false;
  return true;
}

void init_folder(const std::string &cale) {
  struct stat st{};
  if (stat(cale.c_str(), &st) == -1) {
    mkdir(cale.c_str(), 0700);
  }
}

NodeServer::NodeServer(NodeConfig c)
    : conf(std::move(c)), memoriaRam(conf), inelul(conf),
      db_manager(conf.dbPath), monitorul(std::make_shared<loadMonitor>(conf)) {
  log_msg("--- PORNIRE SERVER ---");
  init_folder(conf.targetFilesLocation);

  PeerDescriptor eu;
  eu.ID = conf.nodeId;
  eu.ipAdress = conf.ipAddress;
  eu.port = conf.port;
  inelul.AddNode(eu);
  log_msg("M-am adaugat in inelul de hash.");

  int cnt = 0;
  dirent *e;
  if (DIR *d; (d = opendir(conf.targetFilesLocation.c_str())) != nullptr) {
    while ((e = readdir(d)) != nullptr) {
      if (std::string nume_f = e->d_name; nume_f != "." && nume_f != "..") {
        db_manager.RegisterFile(conf.nodeId, nume_f);
        cnt++;
      }
    }
    closedir(d);
  }
  log_msg("Am indexat " + std::to_string(cnt) + " fisiere locale.");

  db_manager.CreateUser("admin", "admin", "admin");
  db_manager.CreateUser("user", "user", "user");
  db_manager.CreateUser("node_internal", "user", "user");
  log_msg("[AUTH] Sistem autentificare DB initializat.");

  if (!conf.peersVector.empty()) {
    intraInCluster();
  } else {
    log_msg("Sunt Seed/Master ca n-am colegi.");
  }
}

std::unique_ptr<std::string>
NodeServer::citesteDisk(const std::string &fisier) {
  std::string separator = "/";
  if (!conf.targetFilesLocation.empty() &&
      conf.targetFilesLocation.back() == '/')
    separator = "";
  std::string caleCompleta = conf.targetFilesLocation + separator + fisier;

  log_msg("[HDD] Incerc sa citesc: " + caleCompleta);
  std::ifstream f(caleCompleta, std::ios::binary | std::ios::ate);

  if (!f.is_open()) {
    log_msg("[HDD] EROARE: " + caleCompleta + " nu poate fi deschis");
    return nullptr;
  }

  std::streamsize marime = f.tellg();
  if (marime < 0 || static_cast<std::size_t>(marime) > limita_superioara) {
    log_msg("[HDD] Fisier prea mare sau corupt: " + fisier);
    return nullptr;
  }

  log_msg("[HDD] Fisier gasit. Marime: " + std::to_string(marime) +
          " bytes. Incarc in RAM...");
  f.seekg(0, std::ios::beg);
  auto continut = std::make_unique<std::string>(
      static_cast<size_t>(marime), 0);

  if (f.read(&(*continut)[0], marime)) {
    log_msg("[HDD] Citire completa.");
    return continut;
  }
  return nullptr;
}

std::unique_ptr<std::string>
NodeServer::iaDeLaVecin(const PeerDescriptor &vecin,
                        const std::string &ceVreau) {
  log_msg("[NET] Initiez conexiune catre colegul " + vecin.ID + " (" +
          vecin.ipAdress + ":" + std::to_string(vecin.port) + ")");
  try {
    TcpSocket s;
    s.Connect(vecin.port, vecin.ipAdress);

    s.SendAll("AUTH node_internal\n");

    if (const std::string raspunsAuth = s.recvLine(); raspunsAuth.find("ERROR") != std::string::npos) {
      log_msg("[NET] Colegul " + vecin.ID + " a Auth-ul");
      return nullptr;
    }

    log_msg("[NET] Auth OK. Trimit GET " + ceVreau);
    s.SendAll("GET /" + ceVreau + "\n");

    const std::string cap_tabel = s.recvLine(); // absorb header
    std::istringstream stream(cap_tabel);
    std::string proto, stare;
    stream >> proto >> stare;

    if (stare == "OK") {
      std::size_t catDeMare;
      stream >> catDeMare;
      log_msg("[NET] Colegul are fisierul. Marime: " +
              std::to_string(catDeMare));

      if (catDeMare > limita_superioara) {
        log_msg("[NET] Fisierul este prea mare");
        return nullptr;
      }
      std::string datele = s.recvN(catDeMare);
      log_msg("[NET] Transfer complet de la " + vecin.ID);
      return std::make_unique<std::string>(std::move(datele));
    }
  } catch (const std::exception &e) {
    log_msg("[NET] Exception cu colegul (" + vecin.ID + "): " + e.what());
  }
  return nullptr;
}

void NodeServer::log_msg(const std::string &text) {
  std::lock_guard lk(logMutex);
  std::cout << "[" << conf.nodeId << "] " << text << std::endl;
}

loadMonitor::ticket NodeServer::ia_bilet() const { return monitorul->tryAquire(); }

void NodeServer::trimiteLaAltul(const TcpSocket &clientSock) {
  for (const auto vecinii = inelul.Nodes(); const auto &v : vecinii) {
    if (v.ID != conf.nodeId) {
      log_msg("[LOAD] Redirect client catre " + v.ID);
      const std::string msg =
          "RESP REDIRECT " + v.ipAdress + " " + std::to_string(v.port) + "\n";
      try {
        clientSock.SendAll(msg);
      } catch (...) {
      }
      return;
    }
  }
  log_msg("[LOAD] EROARE: Sunt full si n-am vecini!");
  try {
    clientSock.SendAll("RESP ERROR Sunt full si n-am la cine sa te trimit\n");
  } catch (...) {
  }
}
void NodeServer::rezolva_continut(const std::string &nume_resursa, const TcpSocket &s,
                                  const std::string &user_context) {
  std::string ipClient = s.getIP();
  if (ipClient.empty())
    ipClient = "Necunoscut";
  //"admin@127.0.0.1"
  const std::string identitate_log =
      (user_context.empty() ? "anonim" : user_context) + "@" + ipClient;

  log_msg("[REQ] Client " + identitate_log + " vrea: " + nume_resursa);
  db_manager.LogAccess(conf.nodeId, nume_resursa, identitate_log);

  if (const auto dinRam = memoriaRam.Get(nume_resursa)) {
    log_msg("[CACHE] HIT! Am gasit " + nume_resursa + " in RAM (" +
            std::to_string(dinRam->size()) + " bytes).");
    s.SendAll("RESP OK " + std::to_string(dinRam->size()) + "\n");
    s.SendAll(*dinRam);
    log_msg("[CACHE] Trimis instant din memorie.");
    return;
  }

  log_msg("[CACHE] MISS. Nu e in RAM. Intreb baza de date...");

  const std::string idProprietar = db_manager.get_owner_id(nume_resursa);

  PeerDescriptor nodTinta;
  bool eInBaza = false;

  if (!idProprietar.empty()) {
    log_msg("[DB] Baza de date zice ca fisierul e la: " + idProprietar);
    eInBaza = true;

    if (idProprietar == conf.nodeId) {
      log_msg("[DB] Eu sunt proprietarul! Caut pe disc...");
      nodTinta = conf.self();
    } else {
      if (const PeerDescriptor *p = conf.findNode(idProprietar)) {
        nodTinta = *p;
      } else {
        log_msg("[DB] Ciudat... DB zice " + idProprietar +
                " dar nu e online. Recalculez hash.");
        nodTinta = inelul.Locate(nume_resursa);
      }
    }
  } else {
    log_msg("[DB] Fisier nou/necunoscut. HashRing decide cine il tine.");
    nodTinta = inelul.Locate(nume_resursa);
  }

  std::unique_ptr<std::string> dateleMele = nullptr;

  if (nodTinta.ID == conf.nodeId) {
    dateleMele = citesteDisk(nume_resursa);
    if (!dateleMele) {
      log_msg("[DISK] Nu am gasit fisierul pe disc");
      s.SendAll("RESP ERROR Nu e pe disc\n");
      return;
    }
    if (!eInBaza) {
      log_msg("[DB] Inregistrez fisierul nou in baza de date.");
      db_manager.RegisterFile(conf.nodeId, nume_resursa);
    }
  } else {
    log_msg("[PROXY] Nu e la mine. Fac proxy catre " + nodTinta.ID);
    dateleMele = iaDeLaVecin(nodTinta, nume_resursa);

    if (!dateleMele) {
      log_msg("[PROXY] Esec la fetch de la vecin.");
      s.SendAll("RESP ERROR Nu raspunde originea\n");
      return;
    }
  }
  // intai trimit datele, apoi le salvez in cache, din cauza move-ului
  if (dateleMele) {
    const size_t marime = dateleMele->size();
    log_msg("[DONE] Am datele (" + std::to_string(marime) +
            " bytes). Le trimit la client");
    const std::string header = "RESP OK " + std::to_string(marime) + "\n";
    s.SendAll(header);
    s.SendAll(*dateleMele);

    log_msg("[DONE] Trimis cu succes. Acum salvez in Cache RAM.");
    memoriaRam.Put(nume_resursa, *dateleMele);
  }
}

void NodeServer::StartClientLoop(
    TcpSocket socketul, const std::shared_ptr<loadMonitor::ticket>& biletPtr) {
  std::string ip = socketul.getIP();
  log_msg("[CLIENT] Conexiune noua de la " + ip);
  if (!biletPtr || !biletPtr->Valid()) {
    if (auto biletNou = monitorul->tryAquire(); !biletNou.Valid()) {
      log_msg("[LOAD] Resping client " + ip + " (Supraincarcat).");
      trimiteLaAltul(socketul);
      return;
    }
  }

  try {
    std::string rolCurent;
    bool e_logat = false;
    while (true) {
      std::string linie = socketul.recvLine();

      if (linie.empty()) {
        break;
      }

      // log_msg("[DEBUG] Raw command: " + linie);

      std::istringstream iss(linie);
      std::string comanda;
      iss >> comanda;

      std::string restul;
      std::getline(iss, restul);
      // trim()
      if (!restul.empty() && restul[0] == ' ')
        restul.erase(0, 1);
      if (size_t ultimul = restul.find_last_not_of("\r\n "); ultimul != std::string::npos) {
        restul = restul.substr(0, ultimul + 1);
      } else if (!restul.empty()) {
        restul.clear();
      }

      if (comanda == "REGISTER") {
        std::string u, p;
        std::stringstream ss(restul);
        ss >> u >> p;

        if (u.empty() || p.empty()) {
          socketul.SendAll(
              "RESP ERROR Format invalid. Foloseste: register user parola\n");
          continue;
        }

        if (db_manager.CreateUser(u, p, "user")) {
          log_msg("[AUTH] User nou inregistrat: " + u);
          socketul.SendAll("RESP OK CREATED\n");
        } else {
          log_msg("[AUTH] Fail register user: " + u);
          socketul.SendAll("RESP ERROR Userul exista deja\n");
        }
        continue;
      }

      if (comanda == "AUTH") {
        std::string u, p;
        std::stringstream ss(restul);
        ss >> u >> p;

        if (u.empty() || p.empty()) {
          socketul.SendAll("RESP ERROR Format invalid.\n");
          continue;
        }

        if (std::string rol = db_manager.CheckLogin(u, p); !rol.empty()) {
          log_msg(rol.append("[AUTH] Login succes: " + u + " (") + ")");
          e_logat = true;
          rolCurent = rol;
          socketul.SendAll("RESP OK AUTH_ACCEPTED " + rol + "\n");
        } else {
          log_msg("[AUTH] Login esuat: " + u);
          e_logat = false;
          rolCurent = "";
          socketul.SendAll("RESP ERROR User sau parola gresita\n");
        }
        continue;
      }

      auto trebuieAuth = [&](const std::string &c) {
        if (c == "JOIN" || c == "GOSSIP" || c == "PING")
          return false;
        return true;
      };

      if (trebuieAuth(comanda) && !e_logat) {
        log_msg("[SEC] Refuzat comanda " + comanda + " (Neautentificat)");
        socketul.SendAll("RESP ERROR E nevoie de autentificare (AUTH)\n");
        continue;
      }

      if (comanda == "GET") {
        std::string res = restul;
        if (!res.empty() && res[0] == '/')
          res.erase(0, 1);

        if (!e_cale_sigura(res)) {
          log_msg("[SEC] Tentativa path traversal: " + res);
          socketul.SendAll("RESP ERROR Nu ai voie acolo\n");
          continue;
        }
        rezolva_continut(res, socketul, rolCurent);
      }

      else if (comanda == "LOGS") {
        if (rolCurent != "admin") {
          socketul.SendAll("RESP ERROR Nu esti sef (Admin only)\n");
          continue;
        }

        log_msg("[ADMIN] Cineva vrea sa vada log-urile.");
        std::vector<LogEntry> entries = db_manager.getLogs();
        socketul.SendAll("RESP OK LOGS_LIST\n");
        for (const auto &[id, node_id, file_name, client_ip, time] : entries) {
          // conversie urata din chrono in string citibil
          std::time_t timp_t = std::chrono::system_clock::to_time_t(time);
          char buffer_ceas[64];
          // formatare: 2026-01-03 14:30:00
          std::strftime(buffer_ceas, sizeof(buffer_ceas), "%Y-%m-%d %H:%M:%S",
                        std::localtime(&timp_t));

          std::stringstream ss;
          ss << "ID:" << id << " [" << buffer_ceas << "] "
             << "Nod:" << node_id << " " << "Fis:" << file_name
             << " " << "IP:" << client_ip << "\n";

          socketul.SendAll(ss.str());
        }
        socketul.SendAll("RESP END\n");
      } else if (comanda == "JOIN") {
        std::string id, _ip;
        int p;
        std::istringstream joinStream(restul);
        joinStream >> id >> _ip >> p;
        log_msg(ip.append("[CLUSTER] Cerere JOIN de la " + id + " (") + ")");

        bool exista = false;
        for (auto lista = inelul.Nodes(); auto &x : lista)
          if (x.ID == id)
            exista = true;

        if (!exista) {
          inelul.AddNode({id, _ip, static_cast<std::uint16_t>(p)});
          socketul.SendAll("RESP OK WELCOME\n");
        } else {
          socketul.SendAll("RESP OK KNOWN\n");
        }
      }

      else if (comanda == "GOSSIP") {
        std::string id, _ip;
        int p;
        std::istringstream g(restul);
        g >> id >> ip >> p;
        inelul.AddNode({id, _ip, static_cast<std::uint16_t>(p)});
      }

      else if (comanda == "CATALOG") {
        log_msg("[CMD] Userul cere CATALOG.");
        auto c = db_manager.GetCatalog();
        std::string raspuns = "RESP OK CATALOG\n";
        for (auto &[fst, snd] : c)
          raspuns += snd.append(fst + " @ ").append("\n");
        socketul.SendAll(raspuns);
        socketul.SendAll("RESP END\n");
      }

      else if (comanda == "STATS") {
        auto top = db_manager.top_files(5);
        std::string r = "RESP OK STATS\n";
        for (auto &[fst, snd] : top)
          r += fst + " (" + std::to_string(snd) + ")\n";
        socketul.SendAll(r);
        socketul.SendAll("RESP END\n");
      }

      else if (comanda == "PING") {
        socketul.SendAll("RESP OK PONG\n");
      }

      else if (comanda == "PEERS") {
        auto n = inelul.Nodes();
        std::string txt = "RESP OK PEERS_LIST\n";
        txt += "Eu: " + conf.nodeId + "\n";
        for (const auto &x : n) {
          if (x.ID != conf.nodeId)
            txt +=
                x.ID + " @ " + x.ipAdress + ":" + std::to_string(x.port) + "\n";
        }
        socketul.SendAll(txt);
        socketul.SendAll("RESP END\n");
      }

      else if (comanda == "PURGE") {
        if (rolCurent != "admin") {
          socketul.SendAll("RESP ERROR Nu esti sef (Admin only)\n");
          continue;
        }
        if (const std::string& deSters = restul; !deSters.empty()) {
          memoriaRam.Remove(deSters);
          log_msg("[ADMIN] PURGE Cache pentru: " + deSters);
          socketul.SendAll("RESP OK PURGED " + deSters + "\n");
        } else {
          socketul.SendAll("RESP ERROR Ce sterg?\n");
        }
      }

      else if (comanda == "SHUTDOWN") {
        if (rolCurent != "admin") {
          socketul.SendAll("RESP ERROR Nu ai voie\n");
          continue;
        }
        log_msg("[ADMIN] SHUTDOWN primit! Inchid tot...");
        socketul.SendAll("RESP OK SHUTDOWN_INITIATED\n");
        socketul.Close();
        exit(0);
      }

      else {
        log_msg("[CMD] Comanda necunoscuta: " + comanda);
        socketul.SendAll("RESP ERROR Nu stiu comanda asta\n");
      }
    }
  } catch (const std::exception &e) {
    std::string m = e.what();
    if (m.find("Conexiunea s-a incheiat") != std::string::npos ||
        m.find("EOF") != std::string::npos) {
      // Deconectare normala, nu logam eroare
      return;
    }
    log_msg("[NET] Eroare conexiune client: " + m);
  }
}

void NodeServer::intraInCluster() {
  if (conf.peersVector.empty())
    return;
  const auto &s = conf.peersVector[0];
  if (s.ID == conf.nodeId)
    return;

  log_msg("[CLUSTER] Incerc sa intru prin " + s.ID + "...");
  try {
    TcpSocket sock;
    sock.Connect(s.port, s.ipAdress);
    sock.SendAll("JOIN " + conf.nodeId + " " + conf.ipAddress + " " +
                 std::to_string(conf.port) + "\n");
    if (sock.recvLine().find("OK") != std::string::npos)
      log_msg("[CLUSTER] Gata, sunt inauntru.");
  } catch (...) {
    log_msg("[CLUSTER] Nu a mers. Raman singur.");
  }
}