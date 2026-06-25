# Kesser Klimaanlage in Homeassistant einbinden

Ich habe hier eine kleine Anleitung aus verschiedenen Repositories zusammengetragen um Personen einen (hoffentlich) einfachen Weg zu zeigen wie sie (in diesem Fall) eine Kesser Klimaanlage in HomeAssistant integrieren können.

Die Initiale Repository welche ich bei mir selber verwendet hatte ist von https://github.com/lNikazzzl/tcl_ac_esphome
Allerdings ist der Code noch für ESPHome von 2024 und das aktuelle ESPHome meckert ständig rum da sich Klassen und Funktionen in der Zwischenzeit geändert haben. Dazu kommt das veile der Funktionen der Klimaanlage nicht implementiert sind wie z.B. das setzen der Swing States (Lüfter Flügel)

Dann gab es noch die Repository von https://github.com/adaasch/AC-hack
Dort konnte ich alle Befehle finden welche das Tuya Modul am die Klimaanlage sendet.

Da ich nichts mit Python zu tun habe und meine C++ Kentnisse extremst eingerostet sind, habe ich hier ClaudeCode herangezogen und konnte eine funktionierende Version zusammenbauen welche aktuell bei mir zu Hause läuft.


### Anleitung: Initiales Flashen des Tuya-WLAN-Moduls

Zuerst muss das Tuya Modul einmalig mit ESPHome geflashed werden. Dies passiert über einen USB-TTL Adapter. Ich habe mir dafür bei Amazon einen "DSD TECH USB zu TTL Seriell Adapter Konverter" gekauft. 

<img width="1028" height="925" alt="image" src="https://github.com/user-attachments/assets/156af450-6d87-43d9-bcbb-7e78488b726c" />

---

#### 1. Vorbereitung des USB-TTL-Adapters (Lebenswichtig für die Hardware)
Der auf dem Bild gezeigte DSD TECH Adapter muss zwingend auf 3,3 Volt (3.3V) eingestellt werden. Ein Betrieb mit 5 Volt zerstört den Chip auf dem WLAN-Modul sofort.
* Ziehe den kleinen schwarzen Jumper (die Steckbrücke) auf dem Adapter ab.
* Stecke den Jumper so auf die Stifte, dass er **VCC** und **3V3** verbindet (siehe die Grafik unten rechts auf dem Bild).

---

#### 2. Verkabelung herstellen

Die Pins des USB-Adapters müssen mit der sechs-poligen Lötleiste des ausgebauten WLAN-Moduls verbunden werden. Die Datenleitungen zum Senden und Empfangen müssen dabei zwingend über Kreuz verbunden werden.

* Adapter **VCC** wird verbunden mit Platine **3.3V**
* Adapter **GND** wird verbunden mit Platine **GND**
* Adapter **RXD** wird verbunden mit Platine **TXD0**
* Adapter **TXD** wird verbunden mit Platine **RXD0**

<img width="1062" height="667" alt="image" src="https://github.com/user-attachments/assets/dcf39447-fd35-4a22-968a-d7bbc25a72fc" />


---

#### 3. Flash-Modus aktivieren (UART Download Mode)
Damit der Chip auf der Platine eine neue Software annimmt, anstatt die alte zu starten, muss er in den sogenannten Flash-Modus gezwungen werden.
* Verbinde den Pin **BOOT** auf der Platine temporär mit dem Pin **GND** (entweder durch ein angelötetes Kabel oder eine Büroklammer/Drahtbrücke).
* Diese Überbrückung muss zwingend hergestellt sein, **bevor** der USB-Adapter in den Computer gesteckt wird und Strom fließt.

---

#### 4. Firmware initial erstellen
Damtit wir die richtige Firmware mit ESPHome direkt auf die Klimaanlage flashen können benötigen wir zuerst die kompilierte .bin Datei.

Dafür erstellen wir in HomeAssistant unter ESPHome ein neues Gerät, gehen in die "Erweiterten Konfigurationsoptionen" und sagen dort "aus Datei importieren"
Hier geben wir ihm die tcl-ac.yaml.

* NAchdem das Gerät erstellt wurde gehen wir auf die 3 Punkte
* Dort auf Installieren
* Erweiterte Optionen aufklappen
* Firmware Binärdatei herunterladen
* Die Firmware wird kompiliert und kann dann heruntergeladen werden

#### 5. Firmware hochladen
Der eigentliche Upload erfolgt direkt über den Webbrowser.
* Stecke den USB-Adapter in den USB-Anschluss deines Computers (die BOOT-GND-Brücke aus Schritt 3 muss dabei gesetzt sein).
* Öffne im Browser die Webseite **web.esphome.io**.
* Klicke auf den Button **Connect**.
* Wähle im Pop-up-Fenster den erkannten USB-Anschluss (COM-Port) des Adapters aus und klicke auf Verbinden.
* Klicke auf **Install** und wähle im Dateimenü die fertige `.bin`-Datei deiner Firmware aus.
* Warte, bis der Ladebalken 100 % erreicht hat und der Vorgang als erfolgreich bestätigt wird.

Deiser Vorgang ist nur ein Mal nötig. Später kann ein Firmware Update direkt über Homeassistant über das Netzwerk gestartet werden.

---

#### 6. Aufräumen und Rückbau
Nach dem Flashen hängt das Modul weiterhin im Flash-Modus fest. Es muss in den regulären Betriebsmodus zurückversetzt werden.
* Ziehe den USB-Adapter vom Computer ab, um das Modul stromlos zu machen.
* Entferne **alle** angelöteten Kabel und Verbindungen von der Platine.
* Die Brücke zwischen **BOOT** und **GND** muss restlos entfernt werden.
* Setze die Platine zurück in ihr Gehäuse und stecke sie an das Kabel der Klimaanlage.

### Tested on:
- Kesser Split 9000/BTU

### Tuya Module 32001-000140
The [original WiFi-Module](https://github.com/user-attachments/assets/f1888a35-ba68-4869-9790-71ff8c572931) is an ESP8266 and it's original Tuya firmware can be replaced with Tasmota or esphome. It's case is easy to open and [solderpads for serial connection](https://github.com/user-attachments/assets/4515421f-4346-4248-aba7-d4db3886ac40) are available.
The wired UART for the connection to the AC's mainboard uses tx_pin: GPIO15 / rx_pin: GPIO13

### Donation: 
- kaspi kz (outside Russia) 4400430344051161
- sber (Russia) 2202205034977568
