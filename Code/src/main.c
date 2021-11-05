#include "sample.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

int K = 9;  // Anzahl der gewünschten Cluster
int L = 5;  // mögliche Intervall Größe (beispielsweise als 10K wählen -> L = n / 10K)
int LL = 3; // Anzahl Intervalle für die zweite Phase: in jedem Intervall sind dann 2 * K * L / LL Elemente; LL ist Teiler von 2KL      
int FACTOR = 1000000.0; // Skalierung um Wasserstein zu beschleunigen, experimentell bestimmt; keinen Einfluss auf Algorithmus



/*
* Algorithmus zur Bestimmung eines Clusterings 
* @param argv[1]: Name des Ordners der zu gruppierenden Graustufen-Bilder, (ggf. zusätzlich nicht konvertierte Bilder in Unterordner "data")
* @return: Clustering bestehend aus Clusterzentren und in Ordner sortieren Bilder (ggf. zusätzlich sortierte Originalbilder)
*/
int main(int argc, char *argv[])
{   
    // Initialisierung von Parametern

    // zum Auslesen der Elemente eines Ordners
    struct dirent *pDirent = NULL;
    DIR *pDir = NULL;
    struct dirent *pDirent2= NULL;
    DIR *pDir2= NULL;
    struct dirent *pDirent3= NULL;
    DIR *pDir3= NULL;
    result result_algo = 0;

    // für die Zeitmessung
    clock_t start2= 0, end2= 0, start_BUILD= 0, start_PAM= 0, end_PAM= 0, end_BUILD= 0;
    double cpu_time_used2 = 0, time_BUILD= 0, time_PAM= 0;
    double cur_time = 0, max_time = 0, min_time = INFINITY, avg_time = 0;

    // für die Anzahl Auswertungen des Distanzmaßes
    int auswertungen = 0;
    int auswertungen_sample = 0;

    // für die Zählung von Neuzuweisungen
    int weit_dist_zug = 0;
    int weit_dist_zent = 0;

    // für die Kosten
    double total_cost = 0;
    double old_cost = 0;

    int w_sz= 0, h_sz= 0, size = 0, ch_sz= 0; // Breite, Höhe , Gesamtgröße und Anzahl Channels

    // Sicherstellung der korrekten Eingabe
    if (argc != 2)
    {
        printf("Benutzung: ./SampleAlgorithm <Ordnername>\n");
        return 1;
    }

    // Prüfe ob in Phase zwei genug Elemente pro Teilmenge vorhanden sind
    if (K > 2 * K * L / LL)
    {
        printf("Überprüfe Eingabeparameter K,L,LL\n");
        return 1;
    }

    // Prüfe ob LL ein Teiler von  2 * K * L ist
    if ((2 * K * L) % LL != 0)
    {
        printf("Überprüfe Wahl von K,L,LL: LL muss ein Teiler von 2*K*L sein\n");
        return 1;
    }

    printf("K: %d, L: %d, LL: %d\n", K, L, LL);

    // Speichere Eingabeordner
    char *input_dir = argv[1];

    // prüfe ob valider Ordner eingegebn wurde und dieser geöffnet werden kann
    pDir = opendir(input_dir);
    pDir2 = opendir(input_dir);
    pDir3 = opendir(input_dir);
    if (pDir == NULL || pDir2 == NULL || pDir3 == NULL)
    {
        printf("Ordner '%s' kann nicht geöffnet werden \n", argv[1]);
        return 1;
    }

    // bestimme die Bildgröße; es wird angenommen, dass alle Bilder dieselbe Größe haben
    // lade hierzu ein Bild (Bilder werden dadruch erkannt, dass sie ".png" im Namen haben)
    chdir(input_dir);
    while ((pDirent3 = readdir(pDir3)) != NULL)
    {
        if (strstr(pDirent3->d_name, ".png") != NULL)
        {
            unsigned char *img = stbi_load(pDirent3->d_name, &w_sz, &h_sz, &ch_sz, 0); 
            if (img == NULL)
            {
                printf("Error finding the image size \"%s\"\n", pDirent3->d_name);
                exit(1);
            }
            break;
        }
    }
    chdir("..");
    size = w_sz * h_sz;

    // zähle die Elemente des Ordners
    int numb_pics = 0;
    while ((pDirent2 = readdir(pDir2)) != NULL)
    {
        // betrachte nur Bilder
        if (strstr(pDirent2->d_name, ".png") != NULL)
        {
            numb_pics++;
        }
    }
    printf("Es sind %i Elemente im Ordner.\n", numb_pics);

    // Prüfe ob in jedem Intervall mindesten 2*K Objekte liegen
    if (2 * K > numb_pics / L)
    {
        printf("Reduziere die Anzahl der Intervalle, die Intervalle beinhalten nicht genug Bilder um die geforderte Anzahl von Zentren zu bestimmen.\n");
        return 1;
    }

    // Erstelle Array für die Bilder und initialisiere diese
    picture *pictures = (picture *)malloc(numb_pics * sizeof(picture));
    for (int i = 0; i < numb_pics; i++)
    {
        pictures[i].name = (char *)malloc(30 * sizeof(char));
        pictures[i].distance = INFINITY;
    }


    // ii ist der  index für das zu befüllende Bild aus dem Array pictures[]
    int ii = 0;
    // Verarbeite jeden Eintrag
    // readdir gibt einen Pointer zu einem struct der den Eintrag bei der aktuellen Position
    // im Ordner dargestellt durch pDir repräsentiert. Ein NULL pointer wird zurück gegeben, wenn keine weiteren Elemente mehr im Ordner vorhanden sind
    while ((pDirent = readdir(pDir)) != NULL)
    {
        // pDirent->dname beinhaltet den Namen des aktuell betrachteten Element des Ordners
        // betrachte nur Elemente die ".png" Files sind
        if (strstr(pDirent->d_name, ".png") != NULL)
        {
            strcpy(pictures[ii].name, pDirent->d_name); // speichere Namen in Array für die Bilder
            ii++;
        }
    }

    start2 = clock();

    chdir(input_dir); // wechsel in das richtige Verzeichnis zum Öffnen der Bilder und zur Berechnung der Histogramme

    // bestimme Größe der sich nicht überschneidenden Intervalle
    int intervall_size = floor(numb_pics / L);
    int letztes_intervall = numb_pics - (L - 1) * intervall_size; // das letzte Intervall kann größer sein, da Intervalle immer ganze Größe haben
    int end_intervall;
    int w, h, c, sz;
    // reserviere Speicher für die 2*K*L Center und zugehörigen Gewichte, setze Gewichte auf 0 und initialisiere Center mit -1
    int *centers = (int *)malloc(2 * K * L * sizeof(int));
    int *weights = (int *)malloc(2 * K * L * sizeof(int));
    for (int i = 0; i < 2 * K * L; i++)
    {
        centers[i] = -1;
        weights[i] = 0;
    }

    // reserviere Speicher für zwei Histogramme und für die Differenz der beiden Histogramme
    double *histogram1 = (double *)calloc(size, sizeof(double));
    double *histogram2 = (double *)calloc(size, sizeof(double));
    double *histogram_diff = (double *)calloc(size, sizeof(double));

    // Jagged Array für die Distanzen innerhalb des Intervalls, Initialisierung mit -1
    // die Distanzmatrix wird in unterer Dreiecksform gespeichert, Symmetrie und d(x,x) = 0
    double **distances_intervall = malloc(letztes_intervall * sizeof(double *));
    for (int i = 0; i < letztes_intervall; i++)
    {
        distances_intervall[i] = malloc(i * sizeof(double));
        for(int j = 0; j < i; j++)
        {
            distances_intervall[i][j] = -1; // Initialisieriung
        }
    }

    // Erstellung der Konfigurationsdatei für den Clustering Algorithmus
    config configuration;
    configure(&configuration, K, intervall_size);

    for (int L_index = 0; L_index < L; L_index++)
    {
        int n;
        // Bestimme Größe des aktuellen Intervalls, das letzte ist größer
        if (L_index == (L - 1))
        {
            end_intervall = numb_pics;
            // Eckfall falls das letzte Intervall größer ist
            configuration.numb_objs = letztes_intervall;
            n = letztes_intervall;
        }
        else
        {
            end_intervall = (L_index + 1) * intervall_size;
            n = intervall_size;
        }

        // Parameter für Clustering Algorithmus
        // Prüfe ob die Parameter so gewählt sind, dass eine Stichprobe der Größe s gezogen werden kann
        int s = ceil(sqrt(n * K));
        if (s == 0)
        {
            printf("Die Parameter müssen angepasst werden: s = 0 \n");
            return 1;
        }
        else if (s < K)
        {
            printf("Die Parameter müssen angepasst werden: s < K \n");
            return 1;
        }
        else if (s > n)
        {
            printf("Die Parameter müssen angepasst werden: s > intervall_size\n");
            return 1;
        }

        int m = s;
        printf("s = %d, m = %d\n", s, m);

        // das ungewichtete Clustering entspricht dem Clustering bei dem alle Punkte das selbe Gewicht (Gewicht = 1) haben 
        int *unweighted = (int *)malloc(s * sizeof(int));
        for (int i = 0; i < s; i++)
        {
            unweighted[i] = 1;
        }

        // Array für Zufällige Stichprobe aus dem Intervall
        int *RandNum = (int *)calloc(s, sizeof(int));
        get_randnumb(RandNum, s, L_index, intervall_size); // Fülle den Array mit Indizes der zufälligen Bilder

        configuration.numb_objs = s;                       // s Objekte sind in der Stichprobe

        for (int pic1 = 0; pic1 < s; pic1++)
        {
            // Berechne für das Intervall zu jedem Bild das Histogramm und bestimme den Wasserstein Abstand
            histogram_dist(pictures[RandNum[pic1]].name, histogram1, &w, &h, &sz, &c);
            for (int pic2 = 0; pic2 < pic1; pic2++)
            {
                histogram_dist(pictures[RandNum[pic2]].name, histogram2, &w, &h, &sz, &c);
                // Berechne Differenz der Histogramme
                difference_histograms(histogram1, histogram2, histogram_diff, &size);
                // erstelle und löse das Flussproblem
                wasser_wh(w_sz, h_sz, histogram_diff, &distances_intervall[pic1][pic2], &cur_time);
                // Speicherung der Zeit und Zählen der Auswertungen
                avg_time += cur_time;
                if (cur_time < min_time)
                {
                    min_time = cur_time;
                }
                else if (cur_time > max_time)
                {
                    max_time = cur_time;
                }
                auswertungen++;
            }
        }

        configuration.distances = distances_intervall;
        start_BUILD = clock();
        build_weights(&configuration, unweighted); // BUILD-Phase des Clustering-Algorithmus
        end_BUILD = clock();
        start_PAM = clock();
        result_algo = algorithm(&configuration, unweighted); // SWAP-Phase des Clustering-Algorithmus

        end_PAM = clock();
        time_PAM += ((double)(end_PAM - start_PAM)) / CLOCKS_PER_SEC;
        time_BUILD += ((double)(end_BUILD - start_BUILD)) / CLOCKS_PER_SEC;

        if (result_algo == ERROR)
        {
            printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
            return 1;
        }

        // Speichere die Zentren C' im Array centers[]
        for (int i = 0; i < K; i++)
        {
            centers[(L_index * 2 * K) + i] = RandNum[configuration.centers[i]];
        }

        printf("BESITIMMUNG DER WEITESTEN ABSTÄNDE ZENTREN\n");

        // Es muss nur der Abstand der Punkte bestimmt werden, die nicht bereits zu der zufälligen Stichprobe gehörten
        // Die Distanz der Punkte aus der zufälligen Stichprpobe lässt sich im Clustering-Algorithmus bestimmen und ist noch in configuration gespeichert
        for (int i = 0; i < s; i++)
        {
            pictures[RandNum[i]].distance = get_distance(i, configuration.centers[configuration.clusters[i]], &configuration);
            pictures[RandNum[i]].cluster = configuration.clusters[i] + (L_index * 2 * K);
        }

        // bestimme die Abstände der Punkte zu den gefundenen Zentren
        double current_distance;
        for (int picture = L_index * intervall_size; picture < end_intervall; picture++)
        {
            if (pictures[picture].distance == INFINITY) // prüfe ob Distanz schon bearbeitet wurde
            {
                // bestimme Histogramm des Bildes
                histogram_dist(pictures[picture].name, histogram1, &w, &h, &sz, &c);
                for (int center = 0; center < K; center++) // iteriere über alle Zentren
                {
                    int current_center = centers[center + (L_index * 2 * K)];
                    histogram_dist(pictures[current_center].name, histogram2, &w, &h, &sz, &c);
                    // Berechne Differenz der Histogramme
                    difference_histograms(histogram1, histogram2, histogram_diff, &size);
                    // erstelle und löse das Flussproblem
                    wasser_wh(w, h, histogram_diff, &current_distance, &cur_time);
                    avg_time += cur_time;
                    if (cur_time < min_time)
                    {
                        min_time = cur_time;
                    }
                    else if (cur_time > max_time)
                    {
                        max_time = cur_time;
                    }
                    auswertungen++;
                    // speichere die aktuell nächste Distanz und das zugehörige Zentrum
                    if (current_distance < pictures[picture].distance)
                    {
                        pictures[picture].distance = current_distance;

                        pictures[picture].cluster = center + (L_index * 2 * K); // gibt den Index als Element von Center
                    }
                }
            }
        }
        // Finde die m größten Distanzen
        // für L_index = L-1 (letztes Intervall) liegen mehr Bilder im Intervall
        int m_greatest_size;
        if (L_index != L - 1)
        {
            m_greatest_size = intervall_size;
        }
        else
        {
            m_greatest_size = letztes_intervall;
        }
        double m_greatest[m_greatest_size]; // array für die m größten Distanzen mit den Distanzen als Eintrag
        for (int i = L_index * intervall_size; i < end_intervall; i++)
        {
            m_greatest[i - L_index * intervall_size] = pictures[i].distance;
        }

        qsort(m_greatest, intervall_size, sizeof(double), compare); // sortiere den Array

        // bestimme die zu den m größten Distanzen gehörigen Indizes
        int taken_g_ind[numb_pics]; // Array der "anzeigt" ob ein Element schon ausgewählt wurde
        for (int i = 0; i < numb_pics; i++)
        {
            taken_g_ind[i] = i;
        }
        int m_greatest_ind[intervall_size]; // Array für die größten Distanzen
        int ii = 0;
        for (int i = 0; i < m; i++)
        {
            for (int j = L_index * intervall_size; j < end_intervall; j++)
            {
                if (pictures[j].distance == m_greatest[i] && taken_g_ind[j] != -1)
                {
                    m_greatest_ind[ii] = j;
                    ii++;
                    taken_g_ind[j] = -1;
                }
            }
        }
        // Clustere die Punkte mit den m größten Abständen
        configuration.numb_objs = m;
        for (int pic1 = 0; pic1 < m; pic1++)
        {
            // Berechne für das Intervall zu jedem Bild das Histogramm und bestimme den Wasserstein Abstand
            histogram_dist(pictures[m_greatest_ind[pic1]].name, histogram1, &w, &h, &sz, &c);
            for (int pic2 = 0; pic2 < pic1; pic2++)
            {
                histogram_dist(pictures[m_greatest_ind[pic2]].name, histogram2, &w, &h, &sz, &c);
                // Berechne Differenz der Histogramme
                difference_histograms(histogram1, histogram2, histogram_diff, &size);
                // erstelle und löse das Flussproblem
                wasser_wh(w_sz, h_sz, histogram_diff, &distances_intervall[pic1][pic2], &cur_time);
                avg_time += cur_time;
                if (cur_time < min_time)
                {
                    min_time = cur_time;
                }
                else if (cur_time > max_time)
                {
                    max_time = cur_time;
                }
                auswertungen++;
            }
        }

        configuration.distances = distances_intervall;
        start_BUILD = clock();
        build_weights(&configuration, unweighted); // BUILD-Phase des Clustering-Algorithmus
        end_BUILD = clock();
        start_PAM = clock();
        result_algo = algorithm(&configuration, unweighted); // SWAP-Phase des Clustering-Algorithmus
        end_PAM = clock();
        time_PAM += ((double)(end_PAM - start_PAM)) / CLOCKS_PER_SEC;
        time_BUILD += ((double)(end_BUILD - start_BUILD)) / CLOCKS_PER_SEC;

        start_BUILD= clock();

        if (result_algo == ERROR) // Prüfung auf Fehler
        {
            printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
            return 1;
        }
        // Speichere die Zentren C'' im Array centers[]
        for (int i = 0; i < K; i++)
        {
            centers[(L_index * 2 * K) + i + K] = m_greatest_ind[configuration.centers[i]]; 

        }
        // Update die Zuordnung zu den Clustern bezüglich der neuen "weiteste Abstand" Zentren
        int old_cluster;
        double old_distance;
        double new_distance;
        for (int i = 0; i < m; i++)
        {
            old_cluster = pictures[m_greatest_ind[i]].cluster;
            old_distance = pictures[m_greatest_ind[i]].distance;
            // update die Distanz zum nächsten Zentrum und Cluster wenn neues nächstes gefunden wird
            // aus dem Clustering resultierende Distanz zum nächsten Weiteste-Abstände-Zentrum, conf.clusters gibt Cluster Nummer und conf.centers[] Nummer des Zentrums
            // die Abstände wurden bereits vorher berechnet
            new_distance = get_distance(i, configuration.centers[configuration.clusters[i]], &configuration);
            if (pictures[m_greatest_ind[i]].distance > new_distance)
            {
                pictures[m_greatest_ind[i]].distance = new_distance;
                pictures[m_greatest_ind[i]].cluster = (L_index * 2 * K) + K + configuration.clusters[i];
                weit_dist_zent++;
            }
        }
        int current_center;
        bool is_element, is_element_center;
        // prüfe die Zuordnung der Punkte zu den Zentren neu (sind die weitesten Abstände Zentren besser?)
        for (int picture = L_index * intervall_size; picture < end_intervall; picture++) // iteriere über alle Zentren
        {
            // prüfe ob picture ein Zentrum ist oder zu den m weiteste Abstände Zentren gehört
            is_element = check_element(picture, m_greatest_ind, m);
            is_element_center = check_element(picture, centers, 2 * K * L);
            if (is_element == true || is_element_center == true)
            {
                continue;
            }
            histogram_dist(pictures[picture].name, histogram1, &w, &h, &sz, &c);
            for (int center_idx = 0; center_idx < K; center_idx++) // iteriere über alle weitesten Abstand Zentren
            {
                current_center = (L_index * 2 * K) + center_idx + K;
                histogram_dist(pictures[centers[current_center]].name, histogram2, &w, &h, &sz, &c);
                // Berechne Differenz der Histogramme
                difference_histograms(histogram1, histogram2, histogram_diff, &size);
                // erstelle und löse das Flussproblem
                wasser_wh(w_sz, h_sz, histogram_diff, &current_distance, &cur_time);
                avg_time += cur_time;
                if (cur_time < min_time)
                {
                    min_time = cur_time;
                }
                else if (cur_time > max_time)
                {
                    max_time = cur_time;
                }
                auswertungen++;
                if (current_distance < pictures[picture].distance) // prüfe ob gefundene Distanz geringer ist
                {
                    // Aktualisiere die nächste Distanz und das zugehörige Cluster
                    pictures[picture].distance = current_distance;
                    pictures[picture].cluster = current_center;
                    weit_dist_zug++;
                }
            }
        }
        printf("Die durchschnittliche Auswertungszeit bislang ist %f\n", avg_time / auswertungen);
        printf("*********************\n");
        // free allocated memory
        free(RandNum);
        free(unweighted);
    }
    // Bestimme die Gewichte der Zentren
    calc_weights(pictures, weights, numb_pics);

    printf("*********************\n*********************\n");
    printf("Die Zahl der Auswertungen vor dem Clustern der 2 * K * L Zenten ist %d\n", auswertungen);
    auswertungen_sample = auswertungen;
    printf("*********************\n*********************\n");

    // Konfigurationsdatei für den Clustering-Algorithmus für mehrere Zentren
    config multiple_centers_configuration;

    // Array zum Zählen der Elemente in jedem Cluster
    int *cluster_size = (int *)malloc(K * sizeof(int));
    for (int i = 0; i < K; i++)
    {
        cluster_size[i] = 0;
    }

    // für die zweite Phase wird eine weitere Unterteilung der Zentrenmenge (2*K*L Zentren ) in LL Teilmengen durchgeführt
    int intervall_size_LL = 2 * K * L / LL;

    // reserviere Speicher für Gewichte und Center der einzelnen Schritte und Gesamt; initialisiere diese
    int *final_centers = (int *)malloc(K * LL * sizeof(int));             // für die gefundenen Zentren aus Phase2
    int *final_weights = (int *)malloc(K * LL * sizeof(int));             // zugehörige Gewichte
    int *temp_centers = (int *)malloc(intervall_size_LL * sizeof(int));   // akutell bearbeitete Zentren in Phase2
    int *temp_weights = (int *)malloc(intervall_size_LL * sizeof(int));   // zugehörige Gewichte
    int *cluster_updates = (int *)malloc(2 * K * L * sizeof(int));        // zum Updaten der Cluster
    for (int i = 0; i < K * LL; i++)
    {
        final_centers[i] = -1; // Initilialisierung
        final_weights[i] = 0;  // Initilialisierung
    }

    // bestimme für jeden der LL Teilbereiche k Zentren
    for (int LL_index = 0; LL_index < LL; LL_index++)
    {
        // initialisiere Konfigurationsdatei
        configure(&multiple_centers_configuration, K, intervall_size_LL);
        for (int i = 0; i < intervall_size_LL; i++)
        {
            temp_centers[i] = i + LL_index * intervall_size_LL;             // aktuell bearbeitetes Zentrum aus Phase 1
            temp_weights[i] = weights[i + LL_index * intervall_size_LL];    // zugehöriges Gewicht
        }

        // Jagged Array für die Distanzen innerhalb der Teilmenge der Zentren und Initilaisierung
        double *distances_centers[intervall_size_LL];
        for (int i = 0; i < intervall_size_LL; i++)
        {
            distances_centers[i] = malloc(i * sizeof(double));
        }

        // bestimme die paarweise Distanzen der gewählten Zentren
        int cur_cent1;
        int cur_cent2;
        for (int center1 = 0; center1 < intervall_size_LL; center1++)
        {
            cur_cent1 = centers[temp_centers[center1]]; // Zentrum 1 mit dem aktuell gearbeitet wird
            // Berechne für das Intervall zu jedem Bild das Histogramm und bestimme den Wasserstein Abstand
            histogram_dist(pictures[cur_cent1].name, histogram1, &w, &h, &sz, &c);
            for (int center2 = 0; center2 < center1; center2++)
            {
                cur_cent2 = centers[temp_centers[center2]]; // Zentrum 2 mit dem aktuell gearbeitet wird
                histogram_dist(pictures[cur_cent2].name, histogram2, &w, &h, &sz, &c);
                // Berechne Differenz der Histogramme
                difference_histograms(histogram1, histogram2, histogram_diff, &size);
                // erstelle und löse das Flussproblem
                wasser_wh(w_sz, h_sz, histogram_diff, &distances_centers[center1][center2], &cur_time);
                avg_time += cur_time;
                if (cur_time < min_time)
                {
                    min_time = cur_time;
                }
                else if (cur_time > max_time)
                {
                    max_time = cur_time;
                }
                auswertungen++;
            }
        }
        multiple_centers_configuration.distances = distances_centers;

        // bestimme Clustering
        start_BUILD = clock();
        build_weights(&multiple_centers_configuration, temp_weights);  // BUILD-Phase des Clustering-Algorithmus
        end_BUILD = clock();
        start_PAM = clock();
        result_algo = algorithm(&multiple_centers_configuration, temp_weights);  // SWAP-Phase des Clustering-Algorithmus
        end_PAM = clock();
        time_PAM += ((double)(end_PAM - start_PAM)) / CLOCKS_PER_SEC;
        time_BUILD += ((double)(end_BUILD - start_BUILD)) / CLOCKS_PER_SEC;
        if (result_algo == ERROR) // Prüfe ob ein Fehler im Clustering Algorithmus aufgetreten ist
        {
            printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
            return 1;
        }
        // speichere die bestimmten Zentren als Indize von den Bildern (NICHT als Indize der Zentrenmenge centers!)
        for (int i = 0; i < K; i++)
        {
            final_centers[i + K * LL_index] = centers[temp_centers[multiple_centers_configuration.centers[i]]];
        }
        // update cluster Zuordnung der alten Zentren als Indize der neuen Zentren
        for (int i = 0; i < intervall_size_LL; i++) // iteriere über alle bearbeiteten Zentren
        {
            cluster_updates[temp_centers[i]] = multiple_centers_configuration.clusters[i] + K * LL_index;
        }
        // update die Gewichte
        for (int i = 0; i < intervall_size_LL; i++)
        {
            final_weights[cluster_updates[temp_centers[i]]] += weights[temp_centers[i]];
        }
        // gebe paarweise Abstände frei
        for (int i = 0; i < intervall_size_LL; i++)
        {
            free(distances_centers[i]);
        }
    }

    // Konfigurationsdatei für die Bestimmung der finalen Zentrenmenge
    config final_configuration;
    configure(&final_configuration, K, K * LL);

    // Jagged Array für die Speicherung der paarweisen Distanzen
    double *final_distances[K * LL];
    for (int i = 0; i < K * LL; i++)
    {
        final_distances[i] = malloc(i * sizeof(double));
    }

    for (int center1 = 0; center1 < K * LL; center1++)
    {
        // Berechne für das Intervall zu jedem Bild das Histogramm und bestimme den Wasserstein Abstand
        histogram_dist(pictures[final_centers[center1]].name, histogram1, &w, &h, &sz, &c);
        for (int center2 = 0; center2 < center1; center2++)
        {
            histogram_dist(pictures[final_centers[center2]].name, histogram2, &w, &h, &sz, &c);
            // Berechne Differenz der Histogramme
            difference_histograms(histogram1, histogram2, histogram_diff, &size);
            // Erstelle und löse das Flussproblem
            wasser_wh(w_sz, h_sz, histogram_diff, &final_distances[center1][center2], &cur_time);
            avg_time += cur_time;
            if (cur_time < min_time)
            {
                min_time = cur_time;
            }
            else if (cur_time > max_time)
            {
                max_time = cur_time;
            }
            auswertungen++;
        }
    }
    final_configuration.distances = final_distances;
    // bestimme das Clustering
    start_BUILD = clock();
    build_weights(&final_configuration, final_weights);                     // BUUILD-Phase (mit Gewichten)
    end_BUILD = clock();
    start_PAM = clock();
    result_algo = algorithm(&final_configuration, final_weights);  // SWAP-Phase (mit Gewichten)
    end_PAM = clock();
    time_PAM += ((double)(end_PAM - start_PAM)) / CLOCKS_PER_SEC;
    time_BUILD += ((double)(end_BUILD - start_BUILD)) / CLOCKS_PER_SEC;
    if (result_algo == ERROR) // Prüfe auf einen Fehler
    {
        printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
        return 1;
    }
    
    int old_cluster;
    int new_cluster;
    // Update für alle Punkte die Zuorndung nach Phase 2 des Algorithmus (Unterteilung in LL Teilbereiche)
    for (int i = 0; i < numb_pics; i++)
    {
        old_cluster = pictures[i].cluster;
        new_cluster = cluster_updates[old_cluster];
        pictures[i].cluster = new_cluster;
    }

    // Update der Cluster der Bilder nach Clustern der finalen Zentren
    update_clusters(pictures, &final_configuration, numb_pics);

    // vor der Neuzuweisung
    end2 = clock();
    cpu_time_used2 += ((double)(end2 - start2)) / CLOCKS_PER_SEC;
    start2 = clock();
    printf("++++Vor der Neuzuordnung:\n");
    printf("Die Zeit für die Cluster Berechnung ist: %f\n", cpu_time_used2);
    printf("Die Zeit für die Distanzen ist: %f\n", avg_time);
    printf("avg_time:%f, max_time: %f, min_time: %f\n", avg_time / auswertungen, max_time, min_time);
    printf("++++\n");

    // Weise die Punkte ihrem nächsten Zentrum zu (n*k Auswertungen)
    check_zuordnung(pictures, &final_configuration, final_centers, numb_pics, &auswertungen, size, w_sz, h_sz, &avg_time, &total_cost, &old_cost);

    // gebe Cluster als csv file aus
    print_clusters(pictures, &final_configuration, final_centers, numb_pics, cluster_size); // todo: use print

    printf("Die finalen Zentren sind:\n");
    for (int i = 0; i < K; i++)
    {
        printf("Zentrum %d: Bild Nummer %d, Name %s , Anzahl Elemente: %d\n", i, final_centers[final_configuration.centers[i]], pictures[final_centers[final_configuration.centers[i]]].name, cluster_size[i]);
    }
    printf("*********************\n");

    // Sortiere die Bilder in die zugehörigen Ordner
    create_folders(pictures, final_centers, numb_pics, final_configuration);

    // free allocated memory
    free(final_centers);
    free(final_weights);
    free(temp_centers);
    free(temp_weights);
    free(cluster_updates);
    free(final_configuration.centers);
    free(final_configuration.clusters);
    free(final_configuration.second);

    for (int i = 0; i < K * LL; i++)
    {
        free(final_distances[i]);
    }
    end2 = clock();
    cpu_time_used2 += ((double)(end2 - start2)) / CLOCKS_PER_SEC;

    // Ausgabe von Informationen zu der Lösung
    printf("Die alten Kosten sind: %f\n", old_cost /FACTOR);
    printf("Die Gesamt Kosten sind: %f\n", total_cost / FACTOR );
    printf("Die Gesamtzeit für die Cluster Berechnung ist: %f\n", cpu_time_used2);
    printf("Die Gesamtzeit für die Distanzen ist: %f\n", avg_time);
    printf("Die Gesamtzeit imm PAM Algorithmus (BUILD) ist: %f\n", time_BUILD);
    printf("Die Gesamtzeit imm PAM Algorithmus ist: %f\n", time_PAM);
    printf("avg_time:%f, max_time: %f, min_time: %f\n", avg_time / auswertungen, max_time, min_time);
    printf("Die Zahl der Auswertungen vor dem Clustern der 2 * K * L Zenten ist %d\n", auswertungen_sample);
    printf("Die Gesamtzahl von Auswertungen ist: %d\n", auswertungen);
    printf("Die Kontante vor O(nk) ist %f\n", auswertungen / (numb_pics * K * 1.0));
    printf("Es wurden %d Bilder den weitesten Zentren zugewiesen \n", weit_dist_zug);
    printf("Es wurden %d Bilder (der nicht-weitesten) nachträglich den weitesten Zentren zugewiesen\n", weit_dist_zent);

    // Speicher freigeben
    for (int i = 0; i < intervall_size; i++)
    {
        free(distances_intervall[i]);
    }
    free(weights);
    free(centers);
    free(pictures);
    free(configuration.centers);
    free(configuration.clusters);
    free(configuration.second);
    free(multiple_centers_configuration.centers);
    free(multiple_centers_configuration.clusters);
    free(multiple_centers_configuration.second);
    free(cluster_size);

    // Ordner schließen
    closedir(pDir);
    closedir(pDir2);
    closedir(pDir3);

    return 0;
} // Ende main


/*
* Funktion zur Bestimmung der Differenz zwischen zwei Histogrammen
* @param hist1: erstes Histogramm (Array)
* @param hist2: zweites Histogramm (Array)
* @param hist_diff: Differenz der Histogramme (leerer Array)
* @param size: Größe des Histogramms; Länge des Arrays
* @return: paarweise Differenz des Histogramm Einträge im Array hist_diff
*/
void difference_histograms(double *hist1, double *hist2, double *hist_diff, int *size)
{
    double control = 0;
    for (int i = 0; i < *size; i++)
    {
        hist_diff[i] = (hist1[i] - hist2[i]);
        control += hist_diff[i];
    }
    // Falls durch Rechenungenauigkeiten der Kontrollwert kleiner als 0 ist (zu wenig Weiß vorhanden) sollen die Rollen der Bilder getauscht werden,
    // damit Überschuss vorhanden ist. Dieser kann nach Konstruktion des LPs der Wassersteindistanz kostenfrei verschwinden
    // Wenn der Kontrollwert negativ ist, tuasche die Rollen der Bilder; Distanzmaß ist symmetrisch
    if (control < 0)
    {
        for (int j = 0; j < *size; j++)
        {
            hist_diff[j] = -hist_diff[j];
        }
        control = -control;
    }
}

/*
* Funktion zur Initialisierung einer Konfigurationsdatei für die Lokale Suche
* @param conf: nicht initialisierter struct
* @param numb_clusters: Anzahl der zu bestimmenden Cluster/Zentren
* @param numb_objs: Anzahl der zu clusternden Objekte
* @return: initialisierte Konfigurationsdatei (Speicher allockiert,...)
*/
void configure(config *conf, int numb_clusters, int numb_objs)
{
    // Initialisiere Zenten mit -1
    int *start_centers = malloc(numb_clusters * sizeof(int));
    for (int i = 0; i < numb_clusters; i++)
    {
        start_centers[i] = -1;
    }
    // Initialisiere nächstes und zweitnächstes Cluster mit Unendlich
    int *start_clusters = malloc(numb_objs * sizeof(int));
    int *start_second = malloc(numb_objs * sizeof(int));
    for (int i = 0; i < numb_objs; i++)
    {
        start_clusters[i] = INFINITY;
        start_second[i] = INFINITY;
    }
    // Initialisiere restliche Parameter
    conf->tot_it = 0;
    conf->cost = INFINITY;
    conf->numb_objs = numb_objs;
    conf->k = numb_clusters;
    conf->max_it = 9999999;
    conf->tot_it = 0;
    conf->centers = start_centers;
    conf->clusters = start_clusters;
    conf->second = start_second;
}

/*
* Funktion zur Bestimmung von zufälligen Zahlen
* @param RandNum: Array für die zufälligen Zahlen (leer)
* @param s: Anzahl der zu bestimmenden Zahlen
* @param L_index: zur Bestimmung in welchem Bereich die Zahlen liegen sollen
* @param intervall_size: zur Bestimmung in welchem Bereich die Zahlen liegen sollen
* @return: s zufällige Zahlen im Array RandNum
*/
void get_randnumb(int RandNum[], int s, int L_index, int intervall_size)
{
    srand(time(NULL));
    for (int i = 0; i < s; i++)
    {
        bool check = true;
        int number;
        do
        {
            number = rand() % intervall_size + (L_index * intervall_size);
            check = true;
            // überprüfe ob die Zahl bereits im Array vorkommt
            for (int j = 0; j < i; j++)
            {
                if (number == RandNum[j])
                {
                    check = false;
                    break;
                }
            }
        } while (check == false); // bis neues, bisher einzigartiges Element gefunden wurde
        RandNum[i] = number;
    }
}

/*
* Funktion zur Erstellung eines CSV-Files mit der Zuordung der Bilder zu den Clustern
* @param pictures: Struct der Bilder
* @param conf: Konfigurationsdatei des Algorithmus
* @param centers: bestimmte Zentrenmenge
* @param numb_pics: Anzahl Bilder
* @param cluster_size: zum Zählen der Anzahl Elemente in jedem Cluster
* @return: gespeichertes CSV-File
*/
void print_clusters(picture pictures[], config *conf, int *centers, int numb_pics, int *cluster_size)
{
    // öffne csv file
    FILE *csv_ptr;
    csv_ptr = fopen("clusters.csv", "w");
    fprintf(csv_ptr, "cluster number,  center\n");

    for (int i = 0; i < conf->k; i++)
    {
        cluster_size[i] += 1; // das Zentrum ist Teil des Clusters
        int current_center = conf->centers[i];
        char temp[strlen(pictures[centers[current_center]].name)];
        int j = 0;
        for (int r = 0; r != strlen(pictures[centers[current_center]].name); r++)
        {
            if (r < 2)
                continue;

            temp[j++] = pictures[centers[current_center]].name[r];
        }
        temp[j] = 0;
        fprintf(csv_ptr, "%d, %s", i, temp);
        for (int j = 0; j < numb_pics; j++)
        {
            if (pictures[j].cluster == i && centers[current_center] != j)
            {
                // entferne die ersten zwei Einträge des Namen des Bildes, diese sind g_
                int r = 0;
                for (int i = 0; i != strlen(pictures[j].name); i++)
                {
                    if (i < 2)
                        continue;
                    temp[r++] = pictures[j].name[i];
                }
                temp[r] = 0;

                cluster_size[i] += 1; // erhöhe Anzahl der Elemente
                fprintf(csv_ptr, ",");
                fprintf(csv_ptr, "%s", temp);
            }
        }
        fprintf(csv_ptr, "\n");
    }
    fclose(csv_ptr);
}

/*
* Funktion zur Bestimmung Bestimmung der Gewichte der Clusterzentren
* @param pictures: Array der Bilder (benötigt wird pictures[].cluster)
* @param weights: Array für die Gewichte der Zentren (auf 0 initialisiert)
* @param numb_pics: Anzahl Bilder
* @return: Gewichte der Zentern im Array weights
*/
void calc_weights(picture pictures[], int *weights, int numb_pics)
{
    int cluster;
    // Prüfe für jeden Punkt in welchem Zentrum er sich befindet und Update das Gewicht
    for (int i = 0; i < numb_pics; i++)
    {
        cluster = pictures[i].cluster;
        weights[cluster]++;
    }
}

/*
* Funktion zur Zuordnung jedes Bildes zum nächsten Zentrum
* @param pictures: Array der Bilder
* @param conf: Konfigurationsdatei des Algorithmus
* @param numb_pics: Anzahl Bilder
* @return: Aktualisierte Zuordnung der Bilder in pictures[].cluster
*/
void update_clusters(picture pictures[], config *conf, int numb_pics)
{
    int old_cluster;
    int new_cluster;
    for (int i = 0; i < numb_pics; i++)
    {
        // Update das Zentrum des Bildes
        old_cluster = pictures[i].cluster;         // das Bild war zuvor in Cluster old_cluster
        new_cluster = conf->clusters[old_cluster]; // das alte Cluster Nummer old_cluster ist nun Teil von new_cluster
        pictures[i].cluster = new_cluster;         // das Bild befindet sich somit in new_cluster
    }
}
// Prüfe für alle Bilder ob sie dem nächsten Zentrum zugeordnet sind
void check_zuordnung(picture pictures[], config *conf, int *centers, int numb_pics, int *auswertungen, int size, int w_sz, int h_sz, double *avg_time, double *total_cost, double *old_cost)
{
    // Initialisierung
    double current_distance;                                                     // aktuelle Distanz
    double old_distance;                                                         // alte Distanz
    int best_cluster;                                                            // Bestes Cluster
    double best_distance;                                                        // Beste Distanz
    int old_cluster;                                                             // altes Cluster
    int w, h, sz, c;                                                             // Breite, Höhe, Größe, Channel
    double *histogram1 = (double *)calloc(size, sizeof(double));                 // Histogramm für ein Bild
    double *histogram2 = (double *)calloc(size, sizeof(double));                 // Histogramm für ein Bild
    double *histogram_diff = (double *)calloc(size, sizeof(double));             // Differenz zwischen zwei Histogrammen
    double cur_time = 0;                                                         // Zeit der Auswertung
    int neu_zug = 0;                                                             // Anzahl der neu zugewiesenen Bilder

    for (int i = 0; i < numb_pics; i++)
    {
        histogram_dist(pictures[i].name, histogram1, &w, &h, &sz, &c);          // Bestimme das Histogramm des ersten Bildes
        old_cluster = pictures[i].cluster;                                      // speichern der Nummer des alten Clusters                 
        best_distance = INFINITY;                                               // Setze Beste-Distanz auf maximalen Wert
        for (int k_index = 0; k_index < K; k_index++)                           // Iteriere über alle Zentren
        {
            int current_center = conf->centers[k_index];                        // akutelles Zentrum
            histogram_dist(pictures[centers[current_center]].name, histogram2, &w, &h, &sz, &c);
            // Berechne Differenz der Histogramme
            difference_histograms(histogram1, histogram2, histogram_diff, &size);
            // erstelle und löse das Flussproblem
            wasser_wh(w_sz, h_sz, histogram_diff, &current_distance, &cur_time);
            (*avg_time) += cur_time;
            (*auswertungen)++;
            if (old_cluster == k_index)                                         // Speichern der alten besten Distanz
            {
                old_distance = current_distance;
                (*old_cost) += old_distance;                                    // alte Kosten bestimmen
            }
            if (current_distance < best_distance)                               // neue beste Zuordnung gefunden?
            {
                best_distance = current_distance;                               // Updaten der besten Distanz  
                best_cluster = k_index;                                         // Updaten des besten Clusters
            }   
        }
        if (old_distance != best_distance)                                      // wurde das Bild neu zugewiesen?
        {
            neu_zug++;
        }
        pictures[i].cluster = best_cluster;
        (*total_cost) += best_distance;                                         // neue Kosten Bestimmen
    }
    printf("%d Bilder wurden neu zugewiesen.\n", neu_zug);
}


/*
* Funktion zur Prüfung ob eine Zahl in einem Array vorkommt
* @param element: Zahl
* @param array: zu prüfender Array
* @param size: Größe des Arrays
* @return: true, wenn die Zahl vorkommt; sonst false
*/
bool check_element(int element, int *array, int size)
{
    for (int i = 0; i < size; i++)
    {
        if (array[i] == element)
        {
            return true;
        }
    }
    return false;
}

/*
* Funktion zur Sortierung von Kopien der Bilder in die Ordner der Cluster
* @param objects: Struct der zu sortiereden Bilder
* @param centers: Zentrenmenge
* @param numb_pics: Anzahl Bilder
* @param final_configuration: Konfigurationsdatei des Algorithmus
* @return: erstellte Ordner mit sortierten Bildern
*/
int create_folders(picture objects[], int centers[], int numb_pics, config final_configuration)
{
    char temp[50];          // buffer
    char temp_cluster[50];  // buffer
    char temp_cluster2[50]; // buffer
    char temp_cluster3[50]; // buffer
    char temp_name2[50];    // buffer
    int control = 0;        // zur Prüfung ob alle Bilder zugeordnet wurden
    DIR *dir = opendir("clusters");
    if (dir) // Ordner existiert bereits
    {
        printf("Der Ordner clusters exisitiert bereits: die Unterordner der Cluster können nicht erstellt werden.\n");
        return 1;
    }
    else if (ENOENT == errno) // Ordner existiert noch nicht, erstelle den Ordner
    {
        mkdir("./clusters", 0700);
    }
    else // Fehler
    {
        printf("Fehler bei Erstellen des Ordners clusters: die Unterordner der Cluster können nicht erstellt werden.\n");
        return 1;
    }
    chdir("clusters");
    // Erstellung der Ordner (mit dem Namen der Clusternummer) und den Unterordnern für Originalbild und Graustufen
    for (int i = 0; i < K; i++)
    {
        sprintf(temp, "%d", i);   // Schreiben der Clusternummer in den String temp
        mkdir(temp, 0700);        // Ordner erstellen
        chdir(temp);              // in den Ordner wechseln
        mkdir("sw", 0700);        // Unterordner für Graustufenbilder
        mkdir("normal", 0700);    // Unterordner für Original Bilder
        chdir("..");
    }
    chdir("..");
    mkdir("./centers", 0700); // erstelle Ordner für die Zentren
    // kopiere die Zentren in den centers und cluster Ordner, jeweils Graustufen und Normal
    for (int i = 0; i < K; i++)
    {
        sprintf(temp_cluster3, "clusters/%d/sw", i); //Pfad zum Cluster Ordner
        // schwarz weiße Zentren in Zetren Ordner und in Cluster Ordner
        control += copyfile(objects[centers[final_configuration.centers[i]]].name, "centers", i, -1);
        control += copyfile(objects[centers[final_configuration.centers[i]]].name, temp_cluster3, i, -1);
        // normale Zentren
        char temp_name[strlen(objects[centers[final_configuration.centers[i]]].name)];
        int j = 0;
        // Entfernung der ersten Zwei Zeichen, diese sind g_
        for (int r = 0; r != strlen(objects[centers[final_configuration.centers[i]]].name); r++)
        {
            if (r < 2)
                continue;

            temp_name[j++] = objects[centers[final_configuration.centers[i]]].name[r];
        }
        temp_name[j] = 0;
        sprintf(temp_name2, "./%s/%s", "data", temp_name); // Pfad und Name des Normalen Bildes
        int length1, length2;
        for (j = 0; temp_name[j] != '\0'; j++)
        {
            length1 = j;
        }
        for (j = 0; temp_name2[j] != '\0'; j++)
        {
            length2 = j;
        }
        control += copyfile(temp_name2, "centers", i, length2 - length1);       // Kopieren in Zentren Ordner
        sprintf(temp_cluster3, "clusters/%d/normal", i);
        control += copyfile(temp_name2, temp_cluster3, i, length2 - length1);   // Kopieren in zugehörigen Cluster Ordner 
    }

    // kopiere die restlichen Bilder in die Cluster Ordner
    for (int i = 0; i < numb_pics; i++)
    {
        // Pfade für die Bilder
        sprintf(temp_cluster, "%s/%d/sw", "clusters", objects[i].cluster);
        sprintf(temp_cluster2, "%s/%d/normal", "clusters", objects[i].cluster);
        control += copyfile(objects[i].name, temp_cluster, -1, -1);

        // Name für das Bild
        char temp_name[strlen(objects[i].name)];
        int j = 0;
        for (int r = 0; r != strlen(objects[i].name); r++)
        {
            if (r < 2)
                continue;

            temp_name[j++] = objects[i].name[r];
        }
        temp_name[j] = 0;
        sprintf(temp_name2, "./%s/%s", "data", temp_name);

        int length1, length2;
        for (j = 0; temp_name[j] != '\0'; j++)
        {
            length1 = j;
        }
        for (j = 0; temp_name2[j] != '\0'; j++)
        {
            length2 = j;
        }
        int length_input_diff = length2 - length1;
        control += copyfile(temp_name2, temp_cluster2, -1, length_input_diff); // damit nicht Cluster
    }
    chdir("..");
    if (control == 0)
    {
        printf("Alle Bilder wurden erfolgreich kopiert\n");
    }
    else
    {
        printf("Bei %d Bildern ist beim kopieren ein Fehler aufgetreten! \n", control);
    }
    return 0;
}

/*
* Funktion zum Kopieren eines Files an einen andere Ort
* @param infilename: Name des Files
* @param outfileDir: Kopierziel
* @param name_ergaenzt: zur Steuerung ob es sich um ein Zentrum handelt
* @param laenge: ob es sich um das Original oder Graustufenild handelt (Original hat kein "g_" davor)
* @return: paarweise Differenz des Histogramm Einträge im Array hist_diff
*/
int copyfile(char *infilename, char *outfileDir, int name_ergaenzt, int laenge)
{
    // name_ergaenzt gibt für die Zentren an, zu welchem Cluster diese gehören und passt den File Namen dementsprechend an
    FILE *infile;                   // zu kopierendes File
    FILE *outfile;                  // File das geschrieben wird
    int ch;                         // Anzahl Channel
    char outfilename[PATH_MAX];     // Name des zu schreibenden Bildes
    char temp[PATH_MAX];            // zum Speichern des Eingabenamens
    char temp2[PATH_MAX];           

    // Setzen der Namen und Pfade entsprechend Graustufen/Normales Bild und Zentrum oder nicht
    infile = fopen(infilename, "r"); // Öffnen der Einagbefiles
    if (infile == NULL)
    {
        printf("Fehler beim Öffnen von: %s\n", infilename);
        return 1;
    }
    if (name_ergaenzt == -1 && laenge == -1)
    {
        sprintf(outfilename, "%s/%s", outfileDir, infilename);
    }
    else if (name_ergaenzt != -1 && laenge == -1)
    {
        sprintf(outfilename, "%s/center%d_%s", outfileDir, name_ergaenzt, infilename);
    }
    else if (name_ergaenzt != -1 && laenge != -1)
    {
        sprintf(temp, "%s", infilename);

        int j = 0;
        for (int r = 0; r != strlen(temp); r++)
        {
            if (r < laenge)
            {
                continue;
            }

            temp2[j++] = temp[r];
        }
        temp2[j] = 0;
        sprintf(outfilename, "%s/center%d_%s", outfileDir, name_ergaenzt, temp2);
    }
    else if (name_ergaenzt == -1 && laenge != -1)
    {
        sprintf(temp, "%s", infilename);

        int j = 0;
        for (int r = 0; r != strlen(temp); r++)
        {
            if (r < laenge)
            {
                continue;
            }

            temp2[j++] = temp[r];
        }
        temp2[j] = 0;
        sprintf(outfilename, "%s/%s", outfileDir, temp2);
    }
    outfile = fopen(outfilename, "w");  // Öffnen des Ausgabefiles und Fehlerprüfung
    if (outfile == NULL)
    {
        printf("Fehler beim Öffnen von: %s\n", outfilename);
        return 1;
    }
    // Kopieren des Inhalts von Quelle zum Ziel
    while ((ch = fgetc(infile)) != EOF)
    {
        fputc(ch, outfile);
    }
    // Schließen der Dateien
    fclose(infile);
    fclose(outfile);
    return 0;
}

/*
* Funktion zum Bestimmen des Histogrammes eines Graustufen-Bildes
* @param name: Name des Bildes
* @param hist: Array für Histogrammwerte (leer)
* @param width: Breite des Bildes
* @param height: Höhe des Bildes
* @param size: Gesamtgröße des Bildes
* @param channels: Anzahl der Kanäle des Bildes
* @return: paarweise Differenz der Histogramm Einträge im Array hist_diff
*/
void histogram_dist(char *name, double *hist, int *width, int *height, int *size, int *channels)
{
    unsigned long sum_white = 0;                                      // Initialisiere Summe der Weiß-Werte
    unsigned char *img = stbi_load(name, width, height, channels, 0); // Laden des Bildes
    if (img == NULL)
    {
        printf("Fehler beim Laden des Bildes \"%s\"\n", name);
        exit(1);
    }
    *size = *width * *height;                  // Größe des Bildes
    unsigned char *p = img, *pp = img;         // Pointer auf Beginn des Bildes
    
    for (int i = 0; i < *size; i++)            // Iteriere über die Pixel des Bildes
    {
            sum_white += *p;                   // erhöhe Weiß-Summe
            p = p + *channels;                 // gehe channels Schritte weiter für den nächsten Punkt
    }
    for (int i = 0; i < *size; i++)            // Iteriere über die Pixel des Bildes
    {
            hist[i]= *pp * FACTOR / sum_white; // bestimme Histogrammwerte
            pp = pp + *channels;               // gehe channels Schritte weiter für den nächsten Punkt
    }
    stbi_image_free(img); // Speicher freigeben
}

/*
* Algorithmus zum Vergleich zweier Zahlen
* @param a: erste Zahl
* @param b: zweite Zahl
* @return: Integer abhängig davon welche Zahl größer ist
*/
int compare(const void *a, const void *b)
{
    if (*(double *)a < *(double *)b)
        return 1;
    else if (*(double *)a > *(double *)b)
        return -1;
    else
        return 0;
}