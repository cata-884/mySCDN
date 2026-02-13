#include "cdn/NodeServer.hpp"
#include "cdn/Database.hpp"
#include "cdn/LoadMonitor.hpp"
#include "cdn/Types.hpp"
#include <cstddef>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static constexpr std::size_t limita_superioara = 50 * 1024 * 1024;

static bool e_cale_sigura(std::string_view nume) {
  if (nume.empty() || nume[0] == '/')
    return false;
  if (nume.find("..") != std::string_view::npos)
    return false;
  return true;
}

void init_folder(const std::string &cale) {
  struct stat st{};
  if (stat(cale.c_str(), &st) == -1) {
    mkdir(cale.c_str(), 0700);
  }
}

std::string NodeServer::buildFilePath(const std::string &filename) const {
  return conf.targetFilesLocation + "/" + filename;
}

std::optional<PeerDescriptor> NodeServer::findAnyPeer() const {
  for (const auto &p : inelul.Nodes()) {
    if (p.ID != conf.nodeId) {
      return p;
    }
  }
  return std::nullopt;
}

void NodeServer::foreachStorageFile(
    const std::function<void(const std::string &)> &fn) {
  DIR *d = opendir(conf.targetFilesLocation.c_str());
  if (!d)
    return;
  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    std::string filename = dir->d_name;
    if (filename == "." || filename == ".." || dir->d_type == DT_DIR)
      continue;
    fn(filename);
  }
  closedir(d);
}

NodeServer::NodeServer(NodeConfig c)
    : conf(std::move(c)), memoriaRam(conf), inelul(conf),
      db_manager(conf.dbPath), monitorul(std::make_shared<loadMonitor>(conf)) {

  log_msg("--- PORNIRE SERVER (Dynamic Storage) ---");
  init_folder(conf.targetFilesLocation);

  inelul.AddNode(conf.self());

  if (findAnyPeer().has_value()) {
    intraInCluster();
  } else {
    log_msg("Sunt Seed/Master (singur in cluster).");
  }

  db_manager.CreateUser("admin", "admin", "admin");
  db_manager.CreateUser("user", "user", "user");
  db_manager.CreateUser("node_internal", "user", "user");

  RebalanceStorage();
}

void NodeServer::RebalanceStorage() {
  log_msg("[STORAGE] == INCEPE REBALANCING ==");

  int pastrate = 0;
  int ignorate = 0;

  foreachStorageFile([&](const std::string &filename) {
    PeerDescriptor owner = inelul.Locate(filename);

    if (owner.ID == conf.nodeId) {
      db_manager.RegisterFile(conf.nodeId, filename);
      pastrate++;
    } else {
      ignorate++;
    }
  });

  log_msg("[STORAGE] Gata. Inregistrate: " + std::to_string(pastrate) +
          ", Apartinand altor noduri: " + std::to_string(ignorate));
}

bool NodeServer::TransferFileToPeer(const PeerDescriptor &vecin,
                                    const std::string &nume_fisier,
                                    const std::string &content) {
  try {
    TcpSocket s;
    s.Connect(vecin.port, vecin.ipAdress);

    s.SendAll("AUTH node_internal user\n");
    std::string authResp = s.recvLine();
    if (authResp.find("OK") == std::string::npos)
      return false;

    s.SendAll("PUSH " + nume_fisier + " " + std::to_string(content.size()) +
              "\n");
    s.SendAll(content);

    std::string confirmare = s.recvLine();
    return (confirmare.find("SAVED") != std::string::npos);

  } catch (const std::exception &e) {
    log_msg("[NET-ERR] Nu am putut trimite fisierul la " + vecin.ID + ": " +
            e.what());
    return false;
  }
}

void NodeServer::GracefulShutdown() {
  log_msg("[SHUTDOWN] == START HANDOVER ==");
  log_msg("[SHUTDOWN] Am " + std::to_string(inelul.Nodes().size()) +
          " noduri in lista mea.");

  auto vecin_opt = findAnyPeer();
  if (!vecin_opt.has_value()) {
    log_msg("[SHUTDOWN] Sunt singurul nod. Catalogul ramane intact.");
    return;
  }

  const PeerDescriptor &vecin = *vecin_opt;
  log_msg("[SHUTDOWN] Reasignez ownership catre: " + vecin.ID);

  auto catalog = db_manager.GetCatalog();
  for (const auto &[filename, owner] : catalog) {
    if (owner == conf.nodeId) {
      db_manager.RegisterFile(vecin.ID, filename);
      log_msg("[SHUTDOWN] Reasignat: " + filename + " -> " + vecin.ID);
    }
  }

  log_msg("[SHUTDOWN] Gata. Pot sa ma inchid linistit.");
}
std::unique_ptr<std::string>
NodeServer::citesteDisk(const std::string &fisier,
                        const std::string &folder_radacina) {

  std::string root =
      folder_radacina.empty() ? conf.targetFilesLocation : folder_radacina;

  std::string separator = "/";
  if (!root.empty() && root.back() == '/')
    separator = "";

  std::string caleCompleta = root + separator + fisier;

  log_msg("[HDD] Incerc sa citesc din: " + caleCompleta);
  std::ifstream f(caleCompleta, std::ios::binary | std::ios::ate);

  if (!f.is_open()) {

    return nullptr;
  }

  std::streamsize marime = f.tellg();

  if (marime < 0 || static_cast<std::size_t>(marime) > limita_superioara) {
    return nullptr;
  }

  f.seekg(0, std::ios::beg);

  int fd = -1;
  FILE *cfile = nullptr;
#ifdef __linux__
  cfile = fopen(caleCompleta.c_str(), "rb");
  if (cfile) {
    fd = fileno(cfile);
    posix_fadvise(fd, 0, static_cast<off_t>(marime), POSIX_FADV_SEQUENTIAL);
  }
#endif

  auto continut = std::make_unique<std::string>(static_cast<size_t>(marime), 0);
  if (f.read(&(*continut)[0], marime)) {
    if (cfile)
      fclose(cfile);
    return continut;
  }
  if (cfile)
    fclose(cfile);
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

    s.SendAll("AUTH node_internal user\n");

    if (const std::string raspunsAuth = s.recvLine();
        raspunsAuth.find("ERROR") != std::string::npos) {
      log_msg("[NET] Colegul " + vecin.ID + " a Auth-ul");
      return nullptr;
    }

    log_msg("[NET] Auth OK. Trimit GET " + ceVreau);
    s.SendAll("GET /" + ceVreau + "\n");

    const std::string cap_tabel = s.recvLine();
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

loadMonitor::ticket NodeServer::ia_bilet() const {
  return monitorul->tryAquire();
}

void NodeServer::trimiteLaAltul(const TcpSocket &clientSock) {
  if (auto vecin_opt = findAnyPeer()) {
    const auto &v = *vecin_opt;
    log_msg("[LOAD] Redirect client catre " + v.ID);
    const std::string msg =
        "RESP REDIRECT " + v.ipAdress + " " + std::to_string(v.port) + "\n";
    try {
      clientSock.SendAll(msg);
    } catch (...) {
    }
    return;
  }
  log_msg("[LOAD] EROARE: Sunt full si n-am vecini!");
  try {
    clientSock.SendAll("RESP ERROR Sunt full si n-am la cine sa te trimit\n");
  } catch (...) {
  }
}

void NodeServer::rezolva_continut(const std::string &nume_resursa,
                                  const TcpSocket &s,
                                  const std::string &user_context) {
  std::string ipClient = s.getIP();
  if (ipClient.empty())
    ipClient = "Necunoscut";

  std::string clean_user = user_context;

  if (size_t pos = clean_user.find('['); pos != std::string::npos) {
    clean_user = clean_user.substr(0, pos);
  }
  if (clean_user.empty())
    clean_user = "anonim";

  const std::string identitate_log = clean_user + "@" + ipClient;

  log_msg("[REQ] Client " + identitate_log + " vrea: " + nume_resursa);
  db_manager.LogAccess(conf.nodeId, nume_resursa, identitate_log);

  if (const auto dinRam = memoriaRam.Get(nume_resursa)) {
    log_msg("[CACHE] HIT! Trimis din RAM.");
    s.SendAll("RESP OK " + std::to_string(dinRam->size()) + "\n");
    s.SendAll(*dinRam);
    return;
  }

  std::string idProprietar = db_manager.get_owner_id(nume_resursa);
  PeerDescriptor nodTinta;
  bool eInBaza = false;

  if (!idProprietar.empty()) {
    eInBaza = true;
    if (idProprietar == conf.nodeId) {
      nodTinta = conf.self();
    } else {
      bool gasit = false;
      for (const auto &n : inelul.Nodes()) {
        if (n.ID == idProprietar) {
          nodTinta = n;
          gasit = true;
          break;
        }
      }
      if (!gasit) {
        nodTinta = inelul.Locate(nume_resursa);
      }
    }
  } else {
    nodTinta = inelul.Locate(nume_resursa);
  }

  std::unique_ptr<std::string> dateleMele = nullptr;

  if (nodTinta.ID == conf.nodeId) {

    dateleMele = citesteDisk(nume_resursa);

    if (!dateleMele) {
      log_msg("[DISK] 404 - Nu am gasit fisierul in: " +
              conf.targetFilesLocation);
      s.SendAll("RESP ERROR Nu e pe disc\n");
      return;
    }

    if (!eInBaza) {
      log_msg("[DB] Gasit pe disc, inregistrez in DB.");
      db_manager.RegisterFile(conf.nodeId, nume_resursa);
    }
  } else {
    log_msg("[PROXY] Cer de la " + nodTinta.ID);
    dateleMele = iaDeLaVecin(nodTinta, nume_resursa);

    if (!dateleMele && eInBaza) {
      log_msg("[DB] Stale entry pentru " + nume_resursa +
              " (owner=" + nodTinta.ID + "). Sterg si reincerc cu HashRing.");
      db_manager.RemoveFile(nume_resursa);

      PeerDescriptor hashTarget = inelul.Locate(nume_resursa);
      if (hashTarget.ID != nodTinta.ID) {
        if (hashTarget.ID == conf.nodeId) {
          dateleMele = citesteDisk(nume_resursa);
        } else {
          log_msg("[PROXY] Retry via HashRing catre " + hashTarget.ID);
          dateleMele = iaDeLaVecin(hashTarget, nume_resursa);
        }
      }
    }

    if (!dateleMele) {
      log_msg("[DISK] Fallback: incerc citire locala (shared storage).");
      dateleMele = citesteDisk(nume_resursa);
      if (dateleMele) {
        db_manager.RegisterFile(conf.nodeId, nume_resursa);
      }
    }

    if (!dateleMele) {
      s.SendAll("RESP ERROR Nu raspunde originea\n");
      return;
    }
  }

  if (dateleMele) {
    const size_t marime = dateleMele->size();
    log_msg("[DONE] Trimit " + std::to_string(marime) + " bytes.");
    s.SendAll("RESP OK " + std::to_string(marime) + "\n");

    int fileFd = open(buildFilePath(nume_resursa).c_str(), O_RDONLY);
    if (fileFd >= 0) {
      posix_fadvise(fileFd, 0, static_cast<off_t>(marime),
                    POSIX_FADV_SEQUENTIAL);
      if (!s.SendFile(fileFd, marime)) {
        s.SendAll(*dateleMele);
      }
      close(fileFd);
    } else {
      s.SendAll(*dateleMele);
    }

    memoriaRam.Put(nume_resursa, *dateleMele);
  }
}

void NodeServer::StartClientLoop(
    TcpSocket socketul, const std::shared_ptr<loadMonitor::ticket> &biletPtr) {
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
    static const auto trebuieAuth = [](const std::string &c) {
      if (c == "JOIN" || c == "GOSSIP" || c == "PING")
        return false;
      return true;
    };
    while (true) {
      std::string linie = socketul.recvLine();

      if (linie.empty()) {
        break;
      }

      std::istringstream iss(linie);
      std::string comanda;
      iss >> comanda;

      std::string restul;
      std::getline(iss, restul);

      if (!restul.empty() && restul[0] == ' ')
        restul.erase(0, 1);
      if (size_t ultimul = restul.find_last_not_of("\r\n ");
          ultimul != std::string::npos) {
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

          log_msg(rol + " [AUTH] Login succes: " + u);
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

          std::time_t timp_t = std::chrono::system_clock::to_time_t(time);
          char buffer_ceas[64];

          std::strftime(buffer_ceas, sizeof(buffer_ceas), "%Y-%m-%d %H:%M:%S",
                        std::localtime(&timp_t));

          std::stringstream ss;
          ss << "ID:" << id << " [" << buffer_ceas << "] "
             << "Nod:" << node_id << " " << "Fis:" << file_name << " "
             << "IP:" << client_ip << "\n";

          socketul.SendAll(ss.str());
        }
        socketul.SendAll("RESP END\n");
      }

      else if (comanda == "PUSH") {
        std::string nume_fisier;
        std::size_t dimensiune;

        std::stringstream ss(restul);
        ss >> nume_fisier >> dimensiune;

        if (nume_fisier.empty() || dimensiune == 0) {
          socketul.SendAll("RESP ERROR Invalid PUSH format\n");
          continue;
        }

        if (!e_cale_sigura(nume_fisier)) {
          socketul.SendAll("RESP ERROR Path traversal detected\n");
          continue;
        }

        log_msg("[PUSH] Primesc date pentru: " + nume_fisier + " (" +
                std::to_string(dimensiune) + " bytes).");

        std::string datePrimite = socketul.recvN(dimensiune);

        std::string path = buildFilePath(nume_fisier);
        std::ofstream out(path, std::ios::binary);

        if (out.write(datePrimite.data(),
                      static_cast<std::streamsize>(datePrimite.size()))) {
          out.close();

          db_manager.RegisterFile(conf.nodeId, nume_fisier);

          log_msg("[OWNERSHIP] Distributia a fost actualizata. Nodul " +
                  conf.nodeId +
                  " este acum proprietar oficial pentru: " + nume_fisier);

          log_msg("[PUSH] Salvat si inregistrat pe disk: " + nume_fisier);
          socketul.SendAll("RESP OK SAVED\n");
        } else {
          log_msg("[PUSH] Eroare critica la scrierea pe disc: " + nume_fisier);
          socketul.SendAll("RESP ERROR Disk write failed\n");
        }
      }

      else if (comanda == "JOIN") {
        std::string id, _ip;
        int p;
        std::istringstream joinStream(restul);
        joinStream >> id >> _ip >> p;

        log_msg(ip + " [CLUSTER] Cerere JOIN de la " + id);

        bool exista = false;
        for (auto lista = inelul.Nodes(); auto &x : lista)
          if (x.ID == id)
            exista = true;

        if (!exista) {
          PeerDescriptor newcomer{id, _ip, static_cast<std::uint16_t>(p)};
          inelul.AddNode(newcomer);

          for (const auto &peer : inelul.Nodes()) {
            if (peer.ID == conf.nodeId || peer.ID == id)
              continue;
            try {
              TcpSocket gs;
              gs.Connect(peer.port, peer.ipAdress);
              gs.SendAll("GOSSIP " + id + " " + _ip + " " + std::to_string(p) +
                         "\n");
            } catch (...) {
            }
          }

          socketul.SendAll("RESP OK WELCOME\n");
        } else {
          socketul.SendAll("RESP OK KNOWN\n");
        }
      }

      else if (comanda == "GOSSIP") {
        std::string id, _ip;
        int p;
        std::istringstream g(restul);
        g >> id >> _ip >> p;
        inelul.AddNode({id, _ip, static_cast<std::uint16_t>(p)});
      }

      else if (comanda == "CATALOG") {
        log_msg("[CMD] Userul cere CATALOG.");
        auto c = db_manager.GetCatalog();
        std::string raspuns = "RESP OK CATALOG\n";
        for (auto &[nume_fisier, nume_nod] : c) {

          raspuns += nume_fisier + " @ " + nume_nod + "\n";
        }
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
        if (const std::string &deSters = restul; !deSters.empty()) {
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

        log_msg("[ADMIN] SHUTDOWN primit! Initiez protocolul de iesire...");
        socketul.SendAll("RESP OK SHUTDOWN_INITIATED_HANDOVER_STARTED\n");

        GracefulShutdown();

        socketul.Close();
        exit(0);
      }
    }
  } catch (const std::exception &e) {
    std::string m = e.what();
    if (m.find("Conexiunea s-a incheiat") != std::string::npos ||
        m.find("EOF") != std::string::npos) {

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

    sock.SendAll("AUTH node_internal user\n");
    std::string authResp = sock.recvLine();
    if (authResp.find("OK") == std::string::npos) {
      log_msg("[CLUSTER] Auth refuzat de seed.");
      return;
    }

    sock.SendAll("JOIN " + conf.nodeId + " " + conf.ipAddress + " " +
                 std::to_string(conf.port) + "\n");
    if (sock.recvLine().find("OK") == std::string::npos) {
      log_msg("[CLUSTER] JOIN refuzat.");
      return;
    }

    log_msg("[CLUSTER] JOIN acceptat. Sincronizez lista de peers...");
    sock.SendAll("PEERS\n");
    while (true) {
      std::string line = sock.recvLine();
      if (line.empty() || line.find("RESP END") != std::string::npos)
        break;
      if (line.find("RESP OK") != std::string::npos ||
          line.find("Eu:") != std::string::npos)
        continue;
      std::istringstream pss(line);
      std::string peerId, at, ipPort;
      pss >> peerId >> at >> ipPort;
      if (peerId.empty() || ipPort.empty())
        continue;
      auto sep = ipPort.find(':');
      if (sep == std::string::npos)
        continue;
      std::string peerIp = ipPort.substr(0, sep);
      uint16_t peerPort =
          static_cast<uint16_t>(std::stoi(ipPort.substr(sep + 1)));
      inelul.AddNode({peerId, peerIp, peerPort});
    }

    log_msg("[CLUSTER] Sync complet. Am " +
            std::to_string(inelul.Nodes().size()) + " noduri in ring.");
  } catch (...) {
    log_msg("[CLUSTER] Nu a mers. Raman singur.");
  }
}