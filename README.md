# mySCDN

O implementare lightweight de CDN distribuit, scrisă de la zero în C++11 (fără biblioteci externe).

Am construit acest proiect pentru a demonstra concepte de **System Design** pe sisteme legacy (GCC 4.8.5).

### Features
* **Consistent Hashing** pentru distribuția nodurilor.
* **LRU Cache** custom pentru performanță.
* **Thread Pool** pentru conexiuni concurente.
* **Proxy & P2P Fetching** transparent.

### Quick Run
```bash
./run_script.sh 127.0.0.1

### Requirements
Adaugarea unor fisiere mp3 in 'targetFiles'. Daca utilizatorul are cont de spotify, poate folosi 'spotDL'(https://github.com/spotDL/spotify-downloader).