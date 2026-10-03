# FoilMotion Studio 1.9 — pannelli mobili e più schermi

Estrai tutto lo ZIP, poi apri **Avvia_Analisi.cmd** su Windows. Si apre il browser
con un piccolo server locale PowerShell, senza installazioni o Python. Lascia
aperta la finestra di avvio durante l’uso; chiudila per arrestarlo. Il server
ascolta solo su 127.0.0.1 e serve esclusivamente gli asset dell’interfaccia.
CSV e video selezionati restano nel browser.

In alternativa puoi aprire **index.html** direttamente: analisi e percorso
funzionano offline, ma le tile OpenStreetMap richiedono l’avvio HTTP e Internet.
Attiva **Cartografia OpenStreetMap online** nella finestra GPS per caricarle;
l’area visualizzata viene comunicata a OpenStreetMap, non il CSV o il video.
Sono richieste soltanto le tile della vista corrente quando visibile, con cache
browser standard, attribuzione e Referer HTTP. Nessun download offline di tile.

## Pannelli mobili e finestre separate

Assetto, Video/audio, Grafici CSV e Percorso GPS sono quattro pannelli autonomi.

- **Sposta**: trascina la maniglia sopra un altro riquadro per riordinare la pagina.
  In alternativa usa le frecce **← / →**, anche da tastiera.
- **Larghezza**: alterna una colonna e tutta la larghezza della pagina.
- **Finestra**: apre il pannello in una finestra del browser, ridimensionabile e
  trascinabile su un altro monitor. Apri le finestre una alla volta; se il browser
  blocca il popup, consenti i popup per questa pagina e riprova.
- **Riaggancia**: riporta il pannello nel suo posto in pagina. Anche chiudendo
  normalmente la finestra il pannello rientra. **Riaggancia tutto** richiama tutte
  le viste; **Ripristina disposizione** ripristina anche ordine e larghezze.

Carica CSV/video e imposta gli hook una volta nella pagina principale. Ogni
finestra usa gli stessi elementi e lo stesso stato della sessione: non è una copia
indipendente del player. Con **Playback sincrono** attivo, Play/Pausa e seek
mantengono l’allineamento già impostato. Senza hook i cursori restano indipendenti,
come prima: il distacco non inventa una sincronizzazione.

Lo spostamento/riaggancio e il riordino mettono il playback in pausa, conservando
cursori e hook. Riprendi con Play. Le finestre staccate hanno Play/Pausa, stato dei
tempi e Riaggancia. Il Play della finestra video agisce sul video; negli altri
pannelli agisce sui dati (con playback sincrono attivo è lo stesso trasporto).

**Lascia aperta la pagina principale e il launcher.** Chiudere o ricaricare la
pagina principale termina la sessione e chiude le viste staccate. Non aprire altre
copie di index.html per creare viste: usa il pulsante Finestra. Posizioni dei
monitor e disposizione non sono salvate tra riaperture. Il browser può aprire una
scheda invece di una finestra secondo le sue preferenze; puoi trascinarla fuori.

Le finestre visibili contribuiscono all’aggiornamento di un unico orologio. I
browser possono sospendere animazioni quando tutte le finestre sono nascoste o
ridurre la frequenza in risparmio energetico. Il video rimane il riferimento del
playback sincrono; non è garantita una sincronizzazione hardware dei display.

## Nuova disposizione e tempi

- Tre prospetti della tavola a sinistra del video: roll/frontale, pitch/laterale,
  yaw/dall’alto. Ogni box riporta angolo, accelerazione e gyro dell’asse.
- Grafico CSV a scorrimento sotto, esteso sulle due colonne. Su schermi stretti
  i pannelli si dispongono in verticale.
- Convenzione del disegno: X verso prua, Y verso sinistra della tavola, Z verso
  l’alto. Vista frontale dalla prua; vista laterale con prua a destra. Gli angoli
  sono quelli del logger: il widget non compensa montaggi ruotati del sensore.
- Yaw/Z è relativo e deriva nel tempo, non è una bussola. I dati mancanti
  mostrano un trattino e una sagoma attenuata.
- Passa il mouse sul grafico o sui due slider: tempo **HH:MM:SS.mmm**, UTC dati
  quando presente e corrispondenza video/dati quando sono definiti gli hook.
  Sullo slider la posizione del mouse è approssimata alla geometria del cursore
  nativo; dopo il seek il tempo corrente è quello effettivo del player.
- Nei dettagli del video viene letto `moov/mvhd` (versioni 0/1) senza caricare
  l’intero filmato. La data di creazione del contenitore non è necessariamente
  l’inizio della ripresa: **non crea hook automaticamente**. Puoi inserire un
  inizio verificato in ISO 8601 con fuso; verrà mostrato in UTC al passaggio del
  mouse. Gli hook restano il riferimento per il playback sincronizzato.
- I tempi dei fotogrammi sono tempi relativi di presentazione del video; non
  costituiscono automaticamente un’ora assoluta per ogni frame. Non vengono
  interpretati timecode proprietari Insta360 o tracce di timed metadata. Non è
  stato fornito un MP4 esportato da Insta360 per verificarne i metadati reali.

## Cosa cambia

- Riconoscimento automatico del vecchio formato FoilScoot (row_type/device_us)
  e dei nuovi CSV SPORT LOGGER V1.7 / V1.7-diag1 (seq/mono_us).
- Roll, pitch e Z calcolati dal firmware, con e senza correzioni utente;
  accelerazioni, gyro grezzi e corretti, temperatura IMU.
- Temperatura, pressione e umidita' BME; batteria e warning registrato.
- Posizione, velocita', rotta, quota, satelliti e HDOP GPS; percorso su proiezione
  Web Mercator, cartografia OSM opzionale e punto che segue il cursore.
- Riquadri delle misure al cursore, canali raggruppati per sensore, scale per unita'.
- Trigger V1.7 selezionabili per sincronizzare il video e metadati JSON facoltativi.
- Valori invalidi restano assenti; nessun dato GPS viene inventato se manca il modulo.

## Prima prova

1. Premi **Apri CSV** e seleziona il file scaricato dal logger. I formati vengono
   riconosciuti dall'intestazione, senza scegliere una versione manualmente.
2. Facoltativo: apri **Metadati sessione**, premi **Apri JSON** e seleziona insieme
   `<ID>.json` e `<ID>.end.json` della stessa sessione. Servono per setup, chiusura,
   durata completa e origine temporale precisa. Puoi caricare anche uno solo dei due.
   La nuova selezione sostituisce quella precedente, non si aggiunge a essa.
3. Premi **Apri video**. Scegli un trigger nell'elenco e premi **Vai all'evento**.
   Posiziona il video sull'impulso scuro del LED, quindi **Sincronizza questi istanti**.
4. Premi Play su uno dei due pannelli. Riquadri, cursore dati e posizione GPS seguono
   il tempo sincronizzato. Puoi selezionare altre tracce in **Canali e misure**.

Nella cartella **examples** trovi esclusivamente dati sintetici, non una sessione
sportiva reale:

- `S0000000000000001.csv` e i suoi due JSON: circa 30 s, tre trigger, GPS che compare
  dopo 5 s, periodi con IMU/BME/GPS invalidi, un gap di acquisizione, batteria in warning.
- `senza-gps.csv`: circa 5 s di dati da banco senza GPS/UTC, per provare l'assenza del modulo.

Apri gli esempi con il normale pulsante Apri CSV. Non includono un video: puoi
usare un tuo filmato per provare i comandi, ma non esiste una sincronizzazione
fisica corretta fra quel filmato e i dati sintetici.

## Riproduzione e sincronizzazione

Ogni pannello conserva Play, Pausa, Stop, +/-5 secondi, passo precedente/successivo
 e slider. Nei dati il passo segue i campioni registrati; nel video e' 1/FPS.
Imposta gli FPS del filmato (30 predefinito). Spazio alterna play/pausa e le frecce
fanno un passo nel pannello selezionato, fuori dai campi di input.

Un hook corregge l'offset. Per un secondo hook, disattiva Playback sincrono,
posiziona separatamente i cursori su un altro evento lontano dal primo e premi
**+ Hook ai cursori**, poi riattiva Playback sincrono. Due hook correggono la deriva
con una relazione lineare. La riproduzione e' limitata all'intervallo comune.

**Sincronizza questi istanti** sostituisce gli hook e conserva il frame video
selezionato. Attivare Playback sincrono senza hook acquisisce la coppia corrente;
con hook gia' presenti e cursori incompatibili, il player avverte senza spostare
il video. A fine intervallo Play non riparte automaticamente dall'inizio.

Cambiare CSV/sessione/video o applicare/rimuovere metadati azzera gli hook.
Per questo conviene importare i JSON prima di sincronizzare.

L'audio e' udibile durante Play. **Calcola forma d'onda** produce una panoramica
cliccabile, facoltativa, per filmati sotto 350 MB. Se il browser non estrae l'audio,
la riproduzione normale puo' comunque funzionare. Una copia MP4 H.264/AAC e' adatta
ai browser che supportano questi codec; MOV/MP4 da soli non garantiscono il codec.

## Tempi ed eventi V1.7

La timeline usa **mono_us**, non l'ora di arrivo Wi-Fi e non il numero della riga.
L'origine e' ricavata dal primo elapsed_ms; poiche' il firmware tronca i millisecondi,
resta una incertezza inferiore a 1 ms. Con i JSON viene usato start_mono_us esatto.
UTC e' mostrato al cursore se presente. Dove manca nel CSV, puo' essere ricostruito
dai metadati finali ed e' esplicitamente indicato come ricostruito.

Il trigger viene riconosciuto con true/false (anche 1/0). Il riferimento prioritario
e' **trigger_click_mono_us**, cioe' quando l'ESP32 ha accettato il comando. Il player
mostra separatamente il tempo del campione che contiene il trigger. Se il tempo
ricezione e' assente/non valido, usa il campione e avverte. Trigger accodati restano
eventi distinti. L'impulso LED ha la latenza del loop: l'allineamento va controllato
visivamente, non e' una sincronizzazione hardware PPS.

Per il vecchio formato restano device_us, session_s, eventi MARK/LED e separazione
per session_id/boot_id. I vecchi roll/pitch sono stime statiche da accelerometro;
per V1.7 vengono usati gli angoli del firmware, senza ricalcolarli o sottrarre due
volte gli offset. Z resta relativo e soggetto a deriva: non e' una bussola.

## Validita', grafici e metadati

- Campi vuoti/non numerici non diventano zero. imu_valid, bme_valid e gps_fix_valid
  mascherano le rispettive misure; posizione/BME con eta' >=3 s sono esclusi.
  Satelliti e HDOP possono essere disponibili indipendentemente dal fix.
- I riquadri mostrano l'ultimo campione precedente al cursore, senza interpolazione;
  dopo un gap >0,1 s non mantengono indefinitamente le misure V1.7. Il vecchio canale
  batteria asincrono ammette fino a 3 s fra letture.
- Le linee si interrompono ai valori invalidi, ai gap e ai salti di avvolgimento
  degli angoli. Ogni unita' ha la propria scala. Zoom Y, finestra, trascinamento e
  rotella restano disponibili. I canali completamente assenti sono disabilitati.
- BME e GPS ripetono valori fra aggiornamenti fisici: 50 righe/s non significano
  50 fix GPS/s. Il riepilogo riporta il rate effettivo del CSV.
- Il percorso usa Web Mercator; senza tile resta visibile sul fondo scuro.
  Spezza la linea alla perdita del fix; non ricostruisce i tratti mancanti. Non e'
  uno strumento di navigazione e la rotta GPS non indica l'orientamento della tavola.
- I JSON vengono controllati per ID, intervallo e coerenza con elapsed_ms. Conservare
  il nome originale del CSV aiuta il controllo ID. Senza file finale valido, la
  chiusura resta **non verificata**, non viene dichiarata riuscita.
- Un'ultima riga con numero di colonne incompleto viene ignorata con avviso. Righe
  malformate intermedie, timestamp duplicati/non crescenti e sequenze non crescenti
  sono rifiutati. Un troncamento che lascia lo stesso numero di colonne non e'
  sempre rilevabile. Non concatenare file V1.7 di sessioni diverse.
- Warning batteria viene letto dal CSV: nessuna soglia 1S/2S e' inventata dal player.
- File CSV fino a 100 MB: parsing e dati risiedono in RAM. Registrazioni lunghe e
  molti canali nella vista Tutto possono rallentare il browser. Il passo video
  resta temporale, non garantisce il fotogramma adiacente su video a FPS variabile.
- Nessun file originale viene modificato. Hook e preferenze restano solo in memoria.

## Verifiche di questa revisione

Nel repository, dalla cartella FoilMotion:

```sh
node tests/test_player.cjs
node tests/test_player_v17.cjs
node tests/test_player_app.cjs
node tests/test_player_hmi.cjs
node tests/test_player_docking.cjs
```

PASS: parser sul CSV storico reale (17014 campioni), schema V1.7 a 46 colonne,
trigger/tempi, validita', gap, assenza GPS, metadati incompatibili, riga finale
incompleta, percorso e UTC. Il controller UI e' verificato con DOM/canvas/video
simulati: import, riquadri, eventi, hook, entrambi i Play e mantenimento del frame.

Il collaudo visivo e la decodifica di un video nel browser **non sono stati ripetuti**
in questa revisione: il browser integrato non consente l'apertura di URL file locali.
Il precedente test Playwright tests/test_player_sync.cjs resta nel repository per
chi dispone dell'ambiente necessario. I test simulati non provano layout o codec.
Verificato anche il CSV reale S9bba0ab456e392bb (1736 campioni) e una copia con
soli campi GPS sintetici. La cartografia di rete e il layout non sono stati
collaudati visivamente in questa sessione. I test aggiunti verificano prospetti,
segni delle rotazioni, dati assenti, timecode e metadati MP4 sintetici.

## Prova del percorso GPS

Apri `examples/S9bba0ab456e392bb_GPS_SIMULATO.csv` e premi Play dati.
È una copia dei 1736 campioni reali forniti, lunga circa 35 secondi: sono stati
cambiati **solo i campi GPS**. Il percorso è un’ellisse fittizia sul lago di Garda,
con fix a 1 Hz e velocità coerente con la traiettoria. Non è una registrazione
geografica reale. IMU, BME, batteria, trigger e tempi originali sono conservati.
Non associargli i JSON dell’originale, perché il file è una dimostrazione modificata.

La linea futura è grigia; il tratto percorso si colora dal blu (0 m/s) al rosso
(10 m/s o più), secondo la velocità registrata. La velocità assente resta grigia.
Il punto segue i fix del CSV; pausa e seek aggiornano anche il percorso. Non si
collegano periodi privi di fix. La mappa inquadra l’intera sessione automaticamente.
Non include ricerca luoghi, navigazione o download di cartografia.

Riferimenti: [policy tile OpenStreetMap](https://operations.osmfoundation.org/policies/tiles/),
[data di creazione QuickTime](https://developer.apple.com/documentation/quicktime-file-format/movie_header_atom/creation_time),
[timed metadata QuickTime](https://developer.apple.com/documentation/quicktime-file-format/overview_of_timed_metadata).

## Verifica 1.9

Superati i test precedenti e quelli di gestione finestre con DOM simulato:
popup bloccato, più finestre, stesso nodo video e stessa posizione, controlli,
aggiornamenti da finestre secondarie, rientro su chiusura, tooltip, riordino,
larghezze, ripristino e chiusura della finestra principale. Nessun collaudo visivo
multi-monitor o riproduzione video in finestre reali è stato eseguito qui.
Le sorgenti dei test sono nella cartella tests del progetto.

Riferimento tecnico per gli elementi multimediali:
[HTML Standard](https://html.spec.whatwg.org/multipage/media.html).
