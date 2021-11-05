# Masterarbeit Tobias Höschen
Dieses GitHub Repository wurde im Rahmen der Masterarbeit "Clustering von Staubdaten" von Tobias Höschen erstellt und dient als Ergänzung zur Arbeit. Der Code ist gemäß der beigefügten Lizenz verwendbar.
## Inhalt des Repositorys
* Ergebnisse des Clustering, bestehend aus Tabellen der Zuordnung der Bilder und Ordnern der sortierten Bilder
* der vewendete Code, welcher ebenfalls der Arbeit angehangen ist

## Ergebnisse
Die Ausgabe des Algorithmus besteht aus einer Zetnrenmenge und der Zuordnung der Bilder zu diesen Zentren. Für verschiedene Eingaben sind die Ausgaben im Ordner *Ergebnisse* zu fiden. Diese Eingaben sind folgend beschrieben:

* eine Fallstudie im zeitlichen Rahmen einer Woche und einer groben Konvertierung. Der Schwellenwert zur Konvertierung wurde zur Erzeugung diese Ordners mit akz_wert=0,2 als relativ hoch gewählt.
* eine Fallstudie im zeitlichen Rahmen einer Woche und einer feinen Konvertierung. Der Schwellenwert zur Konvertierung wurde zur Erzeugung diese Ordners mit akz_wert=0,15 als tief gewählt.
* der Zeitraum von 6 Monaten und eienr feinen Konvertierung. Der Schwellenwert zur Konvertierung wurde zur Erzeugung diese Ordners mit akz_wert=0,15 als tief gewählt.


## Code

### File Download
Das verwendete Jupyter Notebook zum Download über die API ist *Download_Pictures.ipynb*. Der Benutzer wird aufgefordert wieviele Bilder heruntergeladen werden sollen; Startzeitpunkt, Bildausschnitt und Bildgröße sind zu setzen.

### Konvertierung
Das Programm zur Konvertierung der Bilder besteht aus dem File *grayf* zum Aufruf von *grayscale*. Es wird über alle Bilder iteriert und anhand der Parameter ein Graustufen-Bild erstellt oder das Bild verworfen.

### Clustering
Das Programm besteht aus verschiedenen Dateien, welche im Folgenden kurz beschrieben sind:
* ***main***: der Hauptteil der Algorithmus. Hier wird die Eingabe verarbeitet und die Ausgabe erzeugt. Ebenso werden die anderen Funktionen aufgerufen und Parameter zwischen diesen übergeben. 
* ***algorithm***: der Clusteringalgorithmus für eine Teilmenge der Daten. Mittels einer Lokalen Suche werden Zentren und die Zuordnung de Elemnte zu diesen Zentren bestimmt.
* ***wasser_wh***: zur Bestimmung der Distanz zwischen zwei Bildern. Als Distanzmaß wird hier die Wasserstein-Metrik verwendet, für die ein LP mittels des lizensierten Lösers Gurobi gelöst wird.


## Parameter
Die Parameter für die Algorithmen werden im folgenden aufgeführt und die Funktion knapp erläutert.

### Konvertierung
* ***akz_wert***: der höchste  akzeptierte Wert der Abweichung eines Pixels zu Magenta, welcher akzetiert wird. Ist die Abweichung höher wird ein Pixel schwarz
* ***MULTI***: zur Angleichung des Gewichtes der Bilder. Durch Hinzufügen von Rauschen wird das Gewicht einer jeden Bildes auf *MULTI* Prozent des maximalen Gewichts gebracht
* ***SCALE_FACTOR***: zur Angleichung des Gewichtes der Bilder. Durch Skalierung des Gewichts auf *SCALE_FACTOR* der maximalen Skalierung zur Angleichung gebracht.
* ***AKZ_WHITE***: Gesamtgewicht, das ein Bild haben muss um nicht verworfen zu werden.
* ***AKZ_MAX_WHITE***: Gewicht, dass ein beliebiger Pixel des Bildes haben muss, damit das Bild nicht verworfen wird.

### Clustering
* ***EPSILON***: zur Prüfung der Konvergenz der Lokalen Suche. Verbessert sich die Lösung in einem Iterationschritt nicht um diesen Faktor, so wird der Algorithmus beendet.
* ***FACTOR***: Skalierungsfaktor zur Beschleunigung der Berechnung der Wassersteindistanz, hat keinen Einfluss auf das Ergebnis.
* ***K***: Anazhl der zu bestimmenden Cluster und somit Zentren
* ***L***: Anzahl der Teilbereiche in die in der ersten Phase des Algorithmus unterteilt wird. Hat Einfluss auf die Laufzeit des Algorithmus und auf die Lösung.
* ***LL***: Anzahl der Teilbereiche in die in der zweiten Phase des Algorithmus unterteilt wird. Hat Einfluss auf die Laufzeit des Algorithmus und auf die Lösung.
