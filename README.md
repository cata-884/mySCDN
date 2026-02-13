# mySCDN

O implementare lightweight de **CDN distribuit**, scrisă de la zero în **C++20** (fără biblioteci externe pentru rețea). Proiectul simulează o rețea de noduri care stochează și livrează conținut multimedia, folosind **consistent hashing**, **caching LRU**, **thread pool** și o bază de date **SQLite** pentru logging, catalogare și autentificare.

## Structura Proiectului

```
mySCDN/
├── cdn-server/
│   ├── CMakeLists.txt              # Build system (C++20, warnings stricte)
│   ├── include/
│   │   ├── cdn/
│   │   │   ├── Cache.hpp           # LRU cache cu TTL + shared_mutex
│   │   │   ├── Database.hpp        # SQLite manager + shared_mutex
│   │   │   ├── HashRing.hpp        # Consistent hashing (FNV-1a) + shared_mutex
│   │   │   ├── LoadMonitor.hpp     # Connection limiter (atomic, lock-free)
│   │   │   ├── NodeConfig.hpp      # Configurare nod (CLI args)
│   │   │   ├── NodeServer.hpp      # Logica principală CDN
│   │   │   └── Types.hpp           # PeerDescriptor
│   │   ├── miscellaneous/
│   │   │   ├── ErrorHandling.hpp   # throwIF helper
│   │   │   ├── Security.hpp        # SHA-256 + salt (OpenSSL)
│   │   │   └── ThreadPool.hpp      # Thread pool cu condition_variable
│   │   └── network/
│   │       ├── TcpServer.hpp       # Server TCP (bind, listen, accept)
│   │       └── TcpSocket.hpp       # Socket TCP (buffered I/O, sendfile)
│   ├── src/
│   │   ├── main.cpp                # Entry point + signal handlers
│   │   ├── client.cpp              # Client audio (SDL2 + ncurses)
│   │   ├── cdn/                    # Implementări CDN
│   │   └── network/                # Implementări networking
│   └── scripts/
│       ├── run_script.sh           # Compilare + lansare cluster
│       └── run_clients.sh          # Lansare clienți multipli
├── data/
│   └── storage/                    # Folder partajat pentru fișierele CDN
└── LICENSE                         # GPLv3
```

## Dependențe

### Server

- **CMake** ≥ 3.10
- **GCC/Clang** cu suport **C++20**
- **OpenSSL** (libssl-dev)
- **SQLite3** (libsqlite3-dev)
- **pthreads**

### Client (opțional)

- **SDL2** + **SDL2_mixer** (redare audio)
- **ncurses** (interfață terminal)

## Build Nativ

```bash
cd cdn-server/build
cmake ..
make -j$(nproc)
```

Executabilele rezultate:

- `myscdn_node` — serverul CDN
- `client` — clientul audio (dacă `BUILD_CLIENT=ON`, implicit)

Pentru a compila doar serverul (fără SDL2/ncurses):

```bash
cmake .. -DBUILD_CLIENT=OFF
make -j$(nproc)
```

## Rulare

### Cu scripturi (recomandat)

**1. Pornire cluster (compilare + lansare N noduri)**

```bash
cd cdn-server
./scripts/run_script.sh 127.0.0.1 3
```

Parametrii configurabili în `run_script.sh`:

| Parametru      | Valoare implicită    | Descriere                      |
| -------------- | -------------------- | ------------------------------ |
| `MAX_CONN`     | 20                   | Conexiuni maxime per nod       |
| `CACHE_SIZE`   | 104857600 (100 MB)   | Dimensiune cache LRU           |
| `TTL`          | 3600 (1 oră)         | Time-to-live cache             |
| `DB_PATH`      | `/tmp/myscdn/CDN.db` | Calea bazei de date SQLite     |
| `STORAGE_ROOT` | `mySCDN/data/`       | Directorul partajat de stocare |

Porturile nodurilor: `8000`, ..., `8000 + N - 1`

**2. Pornire clienți**

```bash
# Automat — lansare N clienți
./scripts/run_clients.sh 127.0.0.1 8000 2

# Manual
./build/client 127.0.0.1 8000
```

### Direct (fără scripturi)

```bash
# Nod 1 (seed)
./build/myscdn_node --node-id node1 --listen 127.0.0.1:8000 \
    --target-files ../data/storage --db-path /tmp/myscdn/CDN.db \
    --max-connections 20 --cache-bytes 104857600 --ttl 3600

# Nod 2 (se conectează la nod 1)
./build/myscdn_node --node-id node2 --listen 127.0.0.1:8001 \
    --target-files ../data/storage --db-path /tmp/myscdn/CDN.db \
    --max-connections 20 --cache-bytes 104857600 --ttl 3600 \
    --cluster-node node1@127.0.0.1:8000
```

## Protocol

Comenzile pot fi trimise direct folosind `netcat` (util pentru debugging):

```bash
nc 127.0.0.1 8000
```

| Comandă                    | Permisiuni   | Descriere                                                                         |
| -------------------------- | ------------ | --------------------------------------------------------------------------------- |
| `REGISTER <user> <parola>` | oricine      | Creare cont nou (rol `user`). Returnează `CREATED` sau eroare dacă userul există. |
| `AUTH <user> <parola>`     | oricine      | Autentificare. Returnează rolul (`admin` / `user`).                               |
| `GET /<fisier>`            | autentificat | Descarcă un fișier. Dacă nu e local, proxy sau `REDIRECT`.                        |
| `CATALOG`                  | autentificat | Lista tuturor fișierelor indexate în baza de date.                                |
| `STATS`                    | autentificat | Top 5 cele mai accesate fișiere (din loguri SQLite).                              |
| `PEERS`                    | autentificat | Topologia rețelei (noduri din hash ring).                                         |
| `PING`                     | oricine      | Health check. Răspuns: `PONG`.                                                    |
| `PURGE <fisier>`           | admin        | Șterge fișierul din cache (forțează recitire de pe disc).                         |
| `SHUTDOWN`                 | admin        | Oprește nodul cu graceful shutdown.                                               |
