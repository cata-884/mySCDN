#include "network/TcpSocket.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <chrono>
#include <csignal>
#include <fstream>
#include <ios>
#include <iostream>
#include <ncurses.h>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

std::string readable_time(double secunde) {
  int minute = static_cast<int>(secunde) / 60;
  int sec = static_cast<int>(secunde) % 60;
  char buffer_timp[16];
  snprintf(buffer_timp, sizeof(buffer_timp), "%02d:%02d", minute, sec);
  return std::string(buffer_timp);
}

std::string trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (std::string::npos == first)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

class AudioPlayer {
  Mix_Music *muzica = nullptr;

public:
  AudioPlayer() {

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
      std::cerr << "[Audio] SDL Init Error: " << SDL_GetError() << "\n";
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
      std::cerr << "[Audio] SDL_Mixer Init Error: " << Mix_GetError() << "\n";
  }

  ~AudioPlayer() {
    kill_music();
    Mix_Quit();
    SDL_Quit();
  }

  void kill_music() {
    Mix_HaltMusic();
    if (muzica) {
      Mix_FreeMusic(muzica);
      muzica = nullptr;
    }
  }

  void start_ui_player(const std::string &caleFisier) {
    kill_music();
    muzica = Mix_LoadMUS(caleFisier.c_str());
    if (!muzica) {
      std::cout << "[Audio] Nu pot incarca fisierul (" << caleFisier
                << "): " << Mix_GetError() << "\n";
      return;
    }

    Mix_PlayMusic(muzica, 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    double durata_totala = Mix_MusicDuration(muzica);

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100);

    bool is_paused = false;
    auto ceas_start = std::chrono::steady_clock::now();
    double offset_timp = 0;
    bool running = true;

    while (running && Mix_PlayingMusic()) {
      double timp_acum = offset_timp;

      if (!is_paused) {
        auto now = std::chrono::steady_clock::now();
        timp_acum += std::chrono::duration<double>(now - ceas_start).count();
      }

      clear();
      int h, w;
      getmaxyx(stdscr, h, w);
      size_t last_slash = caleFisier.find_last_of("/\\");
      std::string nume_scurt = (last_slash == std::string::npos)
                                   ? caleFisier
                                   : caleFisier.substr(last_slash + 1);

      std::string titlu = "PLAYING: " + nume_scurt;
      int x_pos = (w - static_cast<int>(titlu.length())) / 2;
      if (x_pos < 0)
        x_pos = 0;
      mvprintw(h / 2 - 2, x_pos, "%s", titlu.c_str());

      int bara_len = w - 20;
      if (bara_len < 10)
        bara_len = 10;

      int filled =
          (durata_totala > 0)
              ? static_cast<int>((timp_acum / durata_totala) * bara_len)
              : 0;
      if (filled > bara_len)
        filled = bara_len;

      mvprintw(h / 2, 10, "[");
      for (int i = 0; i < bara_len; ++i) {
        if (i < filled)
          addch('=');
        else if (i == filled)
          addch('>');
        else
          addch(' ');
      }
      printw("]");

      std::string str_timp =
          readable_time(timp_acum) + " / " +
          (durata_totala > 0 ? readable_time(durata_totala) : "??:??");
      x_pos = (w - static_cast<int>(str_timp.length())) / 2;
      mvprintw(h / 2 + 1, x_pos, "%s", str_timp.c_str());

      std::string status =
          is_paused ? "[ PAUZA ]" : "[ SPACE=Pauza | Q=Stop | Sageti=Seek ]";
      x_pos = (w - static_cast<int>(status.length())) / 2;
      mvprintw(h / 2 + 3, x_pos, "%s", status.c_str());

      refresh();

      int ch = getch();
      if (ch == 'q' || ch == 'Q') {
        running = false;
        kill_music();
      } else if (ch == ' ') {
        if (is_paused) {
          Mix_ResumeMusic();
          is_paused = false;
          ceas_start = std::chrono::steady_clock::now();
        } else {
          Mix_PauseMusic();
          is_paused = true;
          auto now = std::chrono::steady_clock::now();
          offset_timp +=
              std::chrono::duration<double>(now - ceas_start).count();
        }
      } else if (ch == KEY_RIGHT) {
        double t_new = timp_acum + 5.0;
        if (durata_totala > 0 && t_new > durata_totala)
          t_new = durata_totala;
        Mix_SetMusicPosition(t_new);
        offset_timp = t_new;
        ceas_start = std::chrono::steady_clock::now();
      } else if (ch == KEY_LEFT) {
        double t_new = timp_acum - 5.0;
        if (t_new < 0)
          t_new = 0;
        Mix_SetMusicPosition(t_new);
        offset_timp = t_new;
        ceas_start = std::chrono::steady_clock::now();
      }
    }

    endwin();
    std::cout << "[Audio] Redare incheiata.\n";
  }
};

std::string get_nume_simplu(const std::string &cale) {
  size_t it = cale.find_last_of("/\\");
  if (it == std::string::npos)
    return cale;
  return cale.substr(it + 1);
}

std::string DescarcaCeva(std::string &ip_tinta, uint16_t &port,
                         const std::string &file_to_get,
                         const std::string &user, const std::string &pass) {
  int cnt_red = 0;
  const int MAX_TRY = 5;
  const size_t CHUNK_SIZE = 64 * 1024;

  while (cnt_red < MAX_TRY) {
    try {
      TcpSocket s;
      s.Connect(port, ip_tinta);

      if (!user.empty()) {
        s.SendAll("AUTH " + user + " " + pass + "\n");
        std::string dump = s.recvLine();
        if (dump.find("ERROR") != std::string::npos) {
          std::cout << "[Err] Serverul a refuzat auth la download: " << dump
                    << "\n";
          return "";
        }
      }

      s.SendAll("GET " + file_to_get + "\n");

      std::string header_srv = s.recvLine();
      if (header_srv.empty())
        return "";

      std::istringstream iss(header_srv);
      std::string proto, stare;
      iss >> proto >> stare;

      if (stare == "REDIRECT") {
        std::string next_ip;
        uint16_t next_port;
        iss >> next_ip >> next_port;
        std::cout << "[Net] Redirect catre -> " << next_ip << ":" << next_port
                  << "\n";
        ip_tinta = next_ip;
        port = next_port;
        cnt_red++;
        continue;
      }

      if (stare == "OK") {
        size_t marime;
        iss >> marime;
        std::cout << "[Download] " << file_to_get << " (" << marime
                  << " bytes)... " << std::flush;

        std::string cale_out = "/tmp/" + get_nume_simplu(file_to_get);
        std::ofstream fout(cale_out, std::ios::binary);

        size_t primit = 0;
        std::vector<char> buf(CHUNK_SIZE);

        while (primit < marime) {
          size_t ramas = marime - primit;
          size_t cat_cer = (ramas < CHUNK_SIZE) ? ramas : CHUNK_SIZE;

          size_t bytes = s.Recv(buf.data(), cat_cer);
          if (bytes == 0) {
            std::cerr << "\n[Err] Conexiune intrerupta prematur!\n";
            break;
          }
          fout.write(buf.data(), static_cast<std::streamsize>(bytes));
          primit += bytes;
        }

        fout.close();
        if (primit == marime) {
          std::cout << "OK! Salvat ca: " << cale_out << "\n";
          return cale_out;
        } else {
          std::cout << "Esec (marime incorecta)\n";
          return "";
        }
      } else {
        std::string err_msg;
        std::getline(iss, err_msg);
        std::cout << "[Server Err] " << err_msg << "\n";
        return "";
      }

    } catch (std::exception &e) {
      std::cout << "\n[Excep] " << e.what() << "\n";
      return "";
    }
  }
  return "";
}

TcpSocket connectAndAuth(const std::string &ip, uint16_t port,
                         const std::string &user, const std::string &pass) {
  TcpSocket s;
  s.Connect(port, ip);
  if (!user.empty()) {
    s.SendAll("AUTH " + user + " " + pass + "\n");
    s.recvLine();
  }
  return s;
}

void print_help() {
  std::cout << "\n--- Audio Player Client ---\n";
  std::cout << "  register <user> <pass> : Creare cont nou\n";
  std::cout << "  auth <user> <pass>     : Logare\n";
  std::cout << "  catalog                : Lista fisiere\n";
  std::cout << "  get <fisier>           : Descarca (suporta spatii)\n";
  std::cout << "  play                   : Reda ultima melodie\n";
  std::cout << "  quit                   : Iesire\n";
  std::cout << "-------------------------------\n";
}
int main(int argc, char *argv[]) {

  signal(SIGINT, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  if (argc < 2) {
    std::cout << "Utilizare: ./client <IP_SERVER> [PORT_SERVER]\n";
    std::cout << "Default port: 8000\n";
    return 1;
  }

  std::string dest_ip = argv[1];
  uint16_t dest_port =
      (argc >= 3) ? static_cast<uint16_t>(std::stoi(argv[2])) : 8000;

  std::string user_activ, pass_activ;
  AudioPlayer player;
  std::string cmd, last_file;

  print_help();

  while (true) {

    std::string prompt_user = user_activ.empty() ? "guest" : user_activ;
    std::cout << "CDN@" << prompt_user << " (" << dest_ip << ":" << dest_port
              << ") > " << std::flush;

    std::string raw_line;
    if (!std::getline(std::cin, raw_line))
      break;

    raw_line = trim(raw_line);
    if (raw_line.empty())
      continue;

    std::istringstream iss(raw_line);
    iss >> cmd;

    if (cmd == "quit" || cmd == "exit") {
      break;
    }

    try {
      if (cmd == "register") {
        std::string u, p;
        iss >> u >> p;
        if (u.empty() || p.empty()) {
          std::cout << "[Info] Folosire: register <username> <password>\n";
          continue;
        }

        TcpSocket tmp;
        tmp.Connect(dest_port, dest_ip);
        tmp.SendAll("REGISTER " + u + " " + p + "\n");

        std::string r = tmp.recvLine();
        if (r.find("CREATED") != std::string::npos) {
          std::cout << "Cont creat cu succes! Foloseste 'auth " << u
                    << " <parola>' pentru logare.\n";
        } else {
          std::cout << "Eroare la register: " << r << "\n";
        }
      }

      else if (cmd == "auth") {
        std::string u, p;
        iss >> u >> p;
        if (u.empty() || p.empty()) {
          std::cout << "[Info] Folosire: auth <username> <password>\n";
          continue;
        }

        TcpSocket tmp;
        tmp.Connect(dest_port, dest_ip);
        tmp.SendAll("AUTH " + u + " " + p + "\n");

        std::string r = tmp.recvLine();
        if (r.find("AUTH_ACCEPTED") != std::string::npos) {
          user_activ = u;
          pass_activ = p;

          std::stringstream ss_resp(r);
          std::string dummy, rol_final;
          ss_resp >> dummy >> dummy >> dummy >> rol_final;

          std::cout << "Login reusit! Rol: " << rol_final << "\n";
        } else {
          std::cout << "Esec login: " << r << "\n";
        }
      }

      else if (cmd == "catalog") {
        TcpSocket sock =
            connectAndAuth(dest_ip, dest_port, user_activ, pass_activ);

        sock.SendAll("CATALOG\n");

        std::cout << "\n=== CATALOG DISPONIBIL ===\n";
        while (true) {
          std::string line = sock.recvLine();
          if (line.empty() || line.find("RESP END") != std::string::npos)
            break;
          if (line.find("RESP ERROR") != std::string::npos) {
            std::cout << "  >> Eroare server: " << line << "\n";
            break;
          }
          if (line.find("RESP OK") == std::string::npos) {
            std::cout << "  " << line << "\n";
          }
        }
        std::cout << "==========================\n";
      }

      else if (cmd == "get") {
        std::string fname;
        std::getline(iss, fname);
        fname = trim(fname);

        if (fname.empty()) {
          std::cout << "[Info] Specifica numele fisierului.\n";
          continue;
        }

        std::string rezultat =
            DescarcaCeva(dest_ip, dest_port, fname, user_activ, pass_activ);

        if (!rezultat.empty()) {
          last_file = rezultat;
          std::cout << "[Info] Pornesc redarea audio...\n";
          player.start_ui_player(last_file);
        }
      }

      else if (cmd == "play") {
        if (!last_file.empty()) {
          player.start_ui_player(last_file);
        } else {
          std::cout << "[Info] Nu ai descarcat nimic in aceasta sesiune.\n";
        }
      }

      else if (cmd == "shutdown") {
        if (user_activ.empty()) {
          std::cout
              << "[Eroare] Trebuie sa fii logat ca admin pentru shutdown!\n";
          continue;
        }

        TcpSocket adminSock =
            connectAndAuth(dest_ip, dest_port, user_activ, pass_activ);

        adminSock.SendAll("SHUTDOWN\n");
        std::string resp = adminSock.recvLine();
        if (resp.empty()) {
          std::cout << "[Server] Nodul s-a oprit.\n";
        } else {
          std::cout << "[Server] " << resp << "\n";
        }
      }

      else if (cmd == "commands" || cmd == "help") {
        print_help();
      }

      else {
        std::cout
            << "[Info] Comanda necunoscuta. Tasteaza 'help' pentru lista.\n";
      }

    } catch (const std::exception &e) {

      std::cout << "\n[EROARE RETEA] Conexiunea cu nodul " << dest_port
                << " a fost intrerupta.\n";
      std::cout << "Mesaj: " << e.what() << "\n";
      std::cout << "Sfat: Daca ai dat 'shutdown', te poti conecta la alt nod "
                   "folosind 'auth'.\n\n";
    }
  }

  std::cout << "La revedere!\n";
  return 0;
}