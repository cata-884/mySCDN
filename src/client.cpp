#include "network/TcpSocket.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <ios>
#include <ncurses.h> 
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <csignal>
#include <chrono> 

std::string readable_time(double secunde) {
    int minute = static_cast<int>(secunde) / 60;
    int sec = static_cast<int>(secunde) % 60;
    char buffer_timp[16];
    sprintf(buffer_timp, "%02d:%02d", minute, sec);
    return std::string(buffer_timp);
}

class AudioPlayer {
    Mix_Music* muzica = nullptr; 

public:
    AudioPlayer() {
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {} 
        //init mixer
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) 
            std::cerr << "[Audio] Nu a mers init la SDL_Mixer.\n";
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

    void start_ui_player(const std::string& caleFisier) {
        kill_music();
        muzica = Mix_LoadMUS(caleFisier.c_str());
        if (!muzica) {
            std::cout << "[Audio] Eroare load: " << Mix_GetError() << "\n";
            return;
        }

        Mix_PlayMusic(muzica, 1);
        double durata_totala = Mix_MusicDuration(muzica);

        //init ncurses
        initscr();            
        cbreak();             
        noecho();             
        curs_set(0); 
        keypad(stdscr, TRUE); //pentru sageti
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
            getmaxyx(stdscr, h, w); //macro ncurses

            std::string titlu = "PLAYING: " + caleFisier;
            int x_pos = static_cast<int>((static_cast<unsigned>(w) - titlu.length()) / 2);
            mvprintw(h/2 - 2, x_pos, "%s", titlu.c_str());

            //desenam bara
            int bara_len = w - 20;
            if (bara_len < 10) bara_len = 10;
            
            int filled = (durata_totala > 0) ? static_cast<int>((timp_acum / durata_totala) * bara_len) : 0;
            if (filled > bara_len) filled = bara_len;
            
            mvprintw(h/2, 10, "[");
            for(int i=0; i<bara_len; ++i) {
                if(i < filled) addch('=');
                else if(i == filled) addch('>');
                else addch(' ');
            }
            printw("]");

            std::string str_timp = readable_time(timp_acum) + " / " + (durata_totala > 0 ? readable_time(durata_totala) : "??:??");
            x_pos = static_cast<int>((static_cast<unsigned>(w) - str_timp.length())/2);
            mvprintw(h/2 + 1, x_pos, "%s", str_timp.c_str());
            std::string status = is_paused ? "[ PAUZA ]" : "[ SPACE=Pauza | Q=Iesi | Sageti=Seek ]";
            x_pos = static_cast<int>((static_cast<unsigned>(w) - status.length())/2);
            mvprintw(h/2 + 3, x_pos, "%s", status.c_str());

            refresh();
            int ch = getch();
            
            if (ch == 'q' || ch == 'Q') {
                running = false;
                kill_music();
            }
            else if (ch == ' ') {
                if (is_paused) {
                    Mix_ResumeMusic();
                    is_paused = false;
                    ceas_start = std::chrono::steady_clock::now();
                } else {
                    Mix_PauseMusic();
                    is_paused = true;
                    auto now = std::chrono::steady_clock::now();
                    offset_timp += std::chrono::duration<double>(now - ceas_start).count();
                }
            }
            else if (ch == KEY_RIGHT) {
                double t_new = timp_acum + 5.0;
                if (durata_totala > 0 && t_new > durata_totala) t_new = durata_totala;
                
                Mix_SetMusicPosition(t_new);
                offset_timp = t_new;
                ceas_start = std::chrono::steady_clock::now();
            }
            else if (ch == KEY_LEFT) {
                double t_new = timp_acum - 5.0;
                if (t_new < 0) t_new = 0;
                
                Mix_SetMusicPosition(t_new);
                offset_timp = t_new;
                ceas_start = std::chrono::steady_clock::now();
            }
        }

        endwin(); 
        std::cout << "[Audio] Gata melodia.\n";
    }
};

std::string get_nume_simplu(const std::string& cale) {
    size_t it = cale.find_last_of("/\\");
    if (it == std::string::npos) return cale;
    return cale.substr(it + 1);
}

std::string DescarcaCeva(std::string& ip_tinta, uint16_t& port, const std::string& file_to_get, const std::string& cine_cere) {
    int cnt_red = 0;
    const int MAX_TRY = 5;
    const size_t CHUNK_SIZE = 64 * 1024; //pachete de 64KB-optimizează utilizarea cache-ului CPU și a ferestrei TCP 

    while (cnt_red < MAX_TRY) {
        try {
            TcpSocket s;
            s.Connect(port, ip_tinta);
            
            if (!cine_cere.empty()) {
                s.SendAll("AUTH " + cine_cere + "\n");
                //ignoram raspunsul la auth momentan
                std::string dump = s.recvLine();
            }
            s.SendAll("GET " + file_to_get + "\n");

            std::string header_srv = s.recvLine();
            if (header_srv.empty()) return "";

            std::istringstream iss(header_srv);
            std::string proto, stare;
            iss >> proto >> stare;

            if (stare == "REDIRECT") {
                std::string next_ip; uint16_t next_port;
                iss >> next_ip >> next_port;
                std::cout << "[Net] Redirect catre -> " << next_ip << ":" << next_port << "\n";
                ip_tinta = next_ip; 
                port = next_port;
                cnt_red++;
                continue;
            }

            if (stare == "OK") {
                size_t marime; 
                iss >> marime;
                std::cout << "[Download] Iau " << file_to_get << " (" << marime << " bytes)... ";
                std::cout << std::flush;
                
                std::string cale_out = "downloaded_" + get_nume_simplu(file_to_get);
                std::ofstream fout(cale_out, std::ios::binary);
                
                size_t primit = 0;
                std::vector<char> buf(CHUNK_SIZE); 

                while (primit < marime) {
                    size_t ramas = marime - primit;
                    size_t cat_cer = (ramas < CHUNK_SIZE) ? ramas : CHUNK_SIZE;
                    
                    size_t bytes = s.Recv(buf.data(), cat_cer);
                    if (bytes == 0) { 
                        std::cerr << "\n[Err] S-a oprit downloadul!\n"; 
                        break; 
                    }
                    fout.write(buf.data(), static_cast<std::streamsize>(bytes));
                    primit += bytes;
                }
                
                fout.close();
                if (primit == marime) {
                    std::cout << "Gata! (" << cale_out << ")\n";
                    return cale_out;
                } else {
                    return "";
                }
            } else {
                std::string err_msg; 
                std::getline(iss, err_msg);
                std::cout << "[Server zice] " << err_msg << "\n";
                return "";
            }

        } catch (std::exception& e) {
            std::cout << "\n[Err Conexiune] " << e.what() << "\n";
            return "";
        }
    }
    return "";
}

void print_help(){
    std::cout << "\n--- Audio Player ---\n";
    std::cout << "  commands     : Comenzile disponibile\n";
    std::cout << "  get <fisier> : Descarca si canta\n";
    std::cout << "  play         : Reda ultima melodie\n";
    std::cout << "  catalog      : Lista fisiere\n";
    std::cout << "  auth <nume>  : Logare\n";
    std::cout << "  quit         : Terminare sesiune\n";
    std::cout << "--------------------\n";
}

int main(int argc, char* argv[]) {
    signal(SIGINT, SIG_IGN); 
    if (argc != 3) { 
        std::cerr << "Folosire: ./client <IP> <PORT>\n"; 
        return 1; 
    }
    
    std::string dest_ip = argv[1];
    uint16_t dest_port = static_cast<uint16_t>(std::stoi(argv[2]));
    std::string user_activ;

    AudioPlayer player;
    std::string cmd, last_file;

    print_help();

    while (true) {
        std::cout << "CDN (" << dest_ip << ":" << dest_port << ") > ";
        
        std::string raw_line;
        if (!std::getline(std::cin, raw_line)) break;
        if (raw_line.empty()) continue;

        std::istringstream iss(raw_line);
        iss >> cmd; 

        if (cmd == "quit" || cmd == "exit") break;
        
        else if (cmd == "get") {
            std::string fname;
            std::getline(iss, fname); 

            //trim
            size_t p = fname.find_first_not_of(' ');
            if (std::string::npos == p) {
                std::cout << "[Info] Zi si numele fisierului.\n";
                continue;
            }
            fname = fname.substr(p);

            std::string rezultat = DescarcaCeva(dest_ip, dest_port, fname, user_activ);
            
            if (!rezultat.empty()) {
                last_file = rezultat;
                //verificam extensia
                if (rezultat.length() >= 4 && rezultat.substr(rezultat.length() - 4) == ".mp3") {
                    player.start_ui_player(last_file);
                } else {
                    std::cout << "[Info] Nu e mp3, doar l-am salvat.\n";
                }
            }
        }
        else if (cmd == "play") {
            if (!last_file.empty()) 
                player.start_ui_player(last_file);
            else 
                std::cout << "[Info] N-ai descarcat nimic.\n";
        }
        else if (cmd == "catalog") {
            try {
                TcpSocket sock; 
                sock.Connect(dest_port, dest_ip);
                
                //daca avem user, il trimitem
                if (!user_activ.empty()) {
                    sock.SendAll("AUTH " + user_activ + "\n");
                    sock.recvLine(); //ignoram raspunsul de la auth intern
                }
                
                sock.SendAll("CATALOG\n");
                
                std::cout << "\n=== CATALOG ===\n";
                while (true) {
                    std::string line = sock.recvLine();
                    
                    if (line.empty()) break; 
                    if (line.find("RESP END") != std::string::npos) break; 
                    
                    if (line.find("RESP ERROR") != std::string::npos) {
                        std::cout << ">> EROARE SERVER: " << line << "\n";
                        break;
                    }
                    
                    //afisam doar liniile utile (fara headerul protocolului)
                    if (line.find("RESP OK CATALOG") == std::string::npos) {
                        std::cout << line << "\n";
                    }
                }
                std::cout << "===============\n";

            } catch (const std::exception& e) { 
                std::cout << "[Err Net] " << e.what() << "\n"; 
            }
        }
        else if (cmd == "commands"){
            print_help();
        }
        else if (cmd == "auth") {
            std::string u;
            std::getline(iss, u);
            
            size_t p = u.find_first_not_of(' ');
            if (p == std::string::npos) {
                std::cout << "Trebuie sa dai un nume.\n";
                continue;
            }
            u = u.substr(p);

            try {
                TcpSocket tmp;
                tmp.Connect(dest_port, dest_ip);
                
                tmp.SendAll("AUTH " + u + "\n");
                std::string r = tmp.recvLine();
                
                if (r.find("OK") != std::string::npos) {
                    user_activ = u;
                    std::cout << "Te-ai logat ca: " << user_activ << "\n";
                } else {
                    user_activ = ""; 
                    std::cout << "N-a mers auth: " << r << "\n";
                }
                
            } catch (const std::exception& e) {
                std::cout << "[Err] Nu pot verifica userul: " << e.what() << "\n";
            }
        }
        else {
            std::cout << "Nu stiu comanda asta.\n";
        }
    }
    return 0;
}