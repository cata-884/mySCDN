# mySCDN

O implementare lightweight de CDN distribuit, scrisă de la zero în C++11 (fără biblioteci externe pentru rețea). Proiectul simulează o rețea de noduri care stochează și livrează conținut multimedia (MP3), folosind consistent hashing, caching LRU și o bază de date SQLite pentru logging și catalogare.
Readme-ul curent este invechit, dar serverul poate fi oricum rulat folosind configuratia urmatoare. Testarile prin netcat sunt functionale.

## Configurare și Rulare

Sistemul se bazează pe trei scripturi principale pentru configurare și execuție:

**1. Distribuirea Fișierelor**
Acest script populează directoarele nodurilor cu fișiere media pentru a simula distribuția datelor. Este necesar să ai fișiere mp3 în folderul sursă înainte de rulare.

```bash
./scripts/distributeTargetFiles.sh
```

**2. Pornirea Serverelor**
Acest script compilează întregul proiect și lansează nodurile serverului în background. Necesită specificarea adresei IP (de obicei loopback pentru teste locale).

```bash
./scripts/run_script.sh 127.0.0.1 2
```

Serverul are urmatoarele specificatii:

=> Porturile nodurilor se incadreaza in intervalul [8000, 8000+n-1], in raport cu cate servere dorim sa rulam(parametrul n);

=> Fisierele pentru care nodul i este responsabil sunt stocate in locatia ./targetFilesDistributed_i

=> Fiecare nod poate avea maxim 10 conexiuni;

=> Capacitatea cache-ului nodurilor este de 100MB

=> Timpul in care fisierele pot sta in cache este de o ora

Pentru a modifica acesti parametri, trebuie modificat doar run_script.sh.

**3. Pornirea Clienților**
Exista doua opțiuni pentru a rula clienții:

Varianta A: Automat (ruleaza n clienti la [IP] [PORT])

```bash
./scripts/run_clients.sh 127.0.0.1 8000 1
```

Varianta B: Manual

```bash
./build/myscdn_client 127.0.0.1 8001
```

## Comenzi Protocol (Debug via Netcat)

Aceste comenzi pot fi trimise direct către server folosind utilitarul netcat (ex: nc 127.0.0.1 8001). Sunt utile pentru debugging sau administrare.

=> AUTH <nume> Trimite credențialele către server. Exemplu: AUTH admin

=> GET /<nume_fisier> Cere conținutul brut al unui fișier. Dacă fișierul nu este local, serverul poate acționa ca proxy sau poate trimite un mesaj de REDIRECT.

=> CATALOG Returnează lista tuturor fișierelor indexate.

=> STATS Returnează un top al celor mai accesate 5 fișiere, bazat pe log-urile din baza de date SQLite.

=> PEERS Afișează topologia rețelei (ce alte noduri sunt conectate în inelul de hashing).

=> PING Verifică starea serverului. Răspunsul așteptat este PONG.

=> PURGE <nume_fisier> (Doar pentru rolul admin) Șterge un fișier din memoria cache (RAM) a nodului, forțând recitirea de pe disc la următoarea cerere.

=> SHUTDOWN (Doar pentru rolul admin) Oprește procesul serverului în siguranță.
