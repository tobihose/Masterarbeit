#include "sample.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

int K;  // Anzahl der gewünschten Cluster
int L = 1;  // mögliche Intervall Größe (beispielsweise als 10K wählen -> L = n / 10K)
int LL = 2; // Anzahl Intervalle für die zweite Phase: in jedem Intervall sind dann 2 * K * L / LL Elemente; LL ist Teiler von 2KL
            // wähle LL > L^2k/n um für den zweiten Teil weniger als 2 nk Berechnungen zu brauchen


int FACTOR;

// Funktion zum Vergliech welche Zahl größer ist
int compare(const void *a, const void *b)
{
    if (*(double *)a < *(double *)b)
        return 1;
    else if (*(double *)a > *(double *)b)
        return -1;
    else
        return 0;
}

// Hauptfunktion des Programms
// Eingabe: Ordner mit Graustufen Bildern (ggf. zusätzlich nicht konvertierte Bilder in Unterordner "data")
// Ausgabe: Clustering bestehend aus Clusterzentren und in Ordner sortieren Bilder (ggf. zusätzlich sortierte Originalbilder)
int main(int argc, char *argv[])
{
    // Initialisierung von Parametern
    int L_index = 0; //workaround

    // zum Auslesen der Elemente eines Ordners
    struct dirent *pDirent = NULL;
    DIR *pDir = NULL;
    struct dirent *pDirent2 = NULL;
    DIR *pDir2 = NULL;
    struct dirent *pDirent3 = NULL;
    DIR *pDir3 = NULL;
    result result_algo = 0;

    double cur_time;


    // für die Anzahl Auswertungen des Distanzmaßes
    int auswertungen = 0;

    // für die Zählung von Neuzuweisungen
    int weit_dist_zug = 0;
    int weit_dist_zent = 0;


    int w_sz = 0, h_sz = 0, size = 0, ch_sz = 0; // Breite, Höhe , Gesamtgröße und Anzahl Channels

    // Sicherstellung der korrekten Eingabe
    if (argc != 4)
    {
        printf("Benutzung: ./phase1 <Ordnername> <Ausgabename> <K>\n");
        return 1;
    }
    K = atoi(argv[3]);


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

    printf("+++ Starte Phase1 für die ParameterK: %d, L: %d, LL: %d\n", K, L, LL);

    // Speichere Eingabeordner
    char *input_dir = argv[1];

    // speichere Augabefile
    char *output_file = argv[2];

    // prüfe ob valider Ordner eingegebn wurde und dieser geöffnet werden kann
    pDir = opendir(input_dir);
    pDir2 = opendir(input_dir);
    pDir3 = opendir(input_dir);
    if (pDir == NULL || pDir2 == NULL || pDir3 == NULL)
    {
        printf("Ordner '%s' kann nicht geöffnet werden \n", argv[1]);
        return 1;
    }

    // bestimme die Bildgröße; es wird angenommen, dass alle Bilder die selbe Größe haben
    // lade hierzu ein Bild (Bilder werden hier dadruch erkannt, dass sie ".png" im Namen haben)
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
    //printf("Es sind %i Elemente im Ordner.\n", numb_pics);

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
    // im Ordner dargestellt durch pDir repräsentiert. Ein NULL pointer wird zurück gegeben, wenn
    // keine weiteren ELemente mehr im Ordner vorhanden sind
    while ((pDirent = readdir(pDir)) != NULL)
    {
        // pDirent->dname beinhaltet den Namen des aktuell betrachteten Element des Ordners
        // betrachte nur Elemente die ".png" Files sind
        if (strstr(pDirent->d_name, ".png") != NULL)
        {
            strcpy(pictures[ii].name, pDirent->d_name); // speichere Namen in Array für die Bilder
            // printf("pictures[%d].name = %s\n", ii, pictures[ii].name);
            ii++;
        }
    }


    chdir(input_dir); // wechsle ins richtige Verzeichnis zum Öffnen der Bilder und Berechnung der Histogramme

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
        for (int j = 0; j < i; j++)
        {
            distances_intervall[i][j] = -1;
        }
    }

    // Erstellung der Konfigurationsdatei für den Clustering Algorithmus
    config configuration;
    configure(&configuration, K, intervall_size);

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
        // Prüfe ob die Parameter so gewählt sind, dass eine STichprobe der Größe s gezogen werden kann
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
        // printf("s = %d, m = %d\n", s, m);

        // das ungewichtete Clustering entspricht dem Clustering bei dem alle Punkte das selbe Gewicht haben (1)
        int *unweighted = (int *)malloc(s * sizeof(int));
        for (int i = 0; i < s; i++)
        {
            unweighted[i] = 1;
        }

        // array für Zufällige Stichprobe aus dem Intervall
        int *RandNum = (int *)calloc(s, sizeof(int));
        get_randnumb(RandNum, s, L_index, intervall_size); // Fülle den Array mit Indizes der zufälligen Bilder

        configuration.numb_objs = s; // s Objekte sind in der Stichprobe
        for (int pic1 = 0; pic1 < s; pic1++)
        {
            // Berechne für das Intervall zu jedem Bild das Histogramm und bestimme den Wasserstein Abstand
             //printf("picture1: %s\n", pictures[RandNum[pic1]].name);
            histogram_dist(pictures[RandNum[pic1]].name, histogram1, &w, &h, &sz, &c, &size);
            for (int pic2 = 0; pic2 < pic1; pic2++)
            {
                 //printf("picture2: %s\n", pictures[RandNum[pic2]].name);
                histogram_dist(pictures[RandNum[pic2]].name, histogram2, &w, &h, &sz, &c, &size);
                // Berechne Differenz der Histogramme
                difference_histograms(histogram1, histogram2, histogram_diff, &size);

                // erstelle und löse das Flussproblem
                wasser_wh(w_sz, h_sz, histogram_diff, &distances_intervall[pic1][pic2], &cur_time);
                // Speicherung der Zeit und Zählen der Auswertungen
                auswertungen++;
            }
        }

        configuration.distances = distances_intervall;
        build_weights(&configuration, unweighted);
        result_algo = algorithm(&configuration, s, unweighted);

        if (result_algo == ERROR)
        {
            printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
            return 1;
        }

        // Speichere die Zentren C' im Array centers[]
        for (int i = 0; i < K; i++)
        {
            centers[(L_index * 2 * K) + i] = RandNum[configuration.centers[i]];
            // printf("Kontrolle: configuration.centers[i] = %d, RandNum[configuration.centers[i]] = %d\n ", configuration.centers[i], RandNum[configuration.centers[i]]);
            // printf("Zentrum[%d] = %s\n", (L_index * 2 * K) + i, pictures[RandNum[configuration.centers[i]]].name);
            // printf("Zentrum[%d] = %s\n", (L_index * 2 * K) + i, pictures[centers[(L_index * 2 * K) + i]].name);
        }

        // printf("BESITIMMUNG DER WEITESTEN ABSTÄNDE ZENTREN\n");

        // Es muss  nur der Abstand der Punkte bestimmt werden, die nicht bereits zu der zufälligen Stichprobe gehörten
        // Die Distanz der Punkte aus der zufälligen Stichprpobe lässt sich im Clustering Algorithmus bestimmen und ist nnoch in configuration gespeichert
        for (int i = 0; i < s; i++)
        {
            pictures[RandNum[i]].distance = get_distance(i, configuration.centers[configuration.clusters[i]], &configuration);
            pictures[RandNum[i]].cluster = configuration.clusters[i] + (L_index * 2 * K);
        }

        // bestimme die Abstände der Punkte zu den gefundenen Zentren
        double current_distance;
        for (int picture = L_index * intervall_size; picture < end_intervall; picture++)
        {
            if (pictures[picture].distance == INFINITY) // prüfe ob Distanz schon bearbeitet wurden
            {
                // bestimme Histogramm des Bildes
                histogram_dist(pictures[picture].name, histogram1, &w, &h, &sz, &c, &size);

                // iteriere über alle Zentren
                for (int center = 0; center < K; center++)
                {
                    int current_center = centers[center + (L_index * 2 * K)];

                    histogram_dist(pictures[current_center].name, histogram2, &w, &h, &sz, &c, &size);

                    // Berechne Differenz der Histogramme
                    difference_histograms(histogram1, histogram2, histogram_diff, &size);

                    // erstelle und löse das Flussproblem
                    wasser_wh(w, h, histogram_diff, &current_distance, &cur_time);

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

        qsort(m_greatest, intervall_size, sizeof(double), compare); // sorrtiere den Array

        // printf("*********************\n");

        // bestimme die zu den m größten Distanzen gehörigen Indizes
        int taken_g_ind[numb_pics]; // Array der "anzeigt" ob ein Element schon ausgewählt wurde
        for (int i = 0; i < numb_pics; i++)
        {
            taken_g_ind[i] = i;
        }
        int m_greatest_ind[intervall_size]; // Array für die größten Distanzen
        ii = 0;
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
            histogram_dist(pictures[m_greatest_ind[pic1]].name, histogram1, &w, &h, &sz, &c, &size);
            for (int pic2 = 0; pic2 < pic1; pic2++)
            {
                histogram_dist(pictures[m_greatest_ind[pic2]].name, histogram2, &w, &h, &sz, &c, &size);
                // Berechne Differenz der Histogramme
                difference_histograms(histogram1, histogram2, histogram_diff, &size);
                // erstelle und löse das Flussproblem
                wasser_wh(w_sz, h_sz, histogram_diff, &distances_intervall[pic1][pic2], &cur_time);
            }
        }

        configuration.distances = distances_intervall;
        build_weights(&configuration, unweighted); // BUILD Phase des Clustering Algorithmus
        result_algo = algorithm(&configuration, m, unweighted); // SWAP Phase des Clustering Algorithmus


        if (result_algo == ERROR) // Prüfung auf Fehler
        {
            printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
            return 1;
        }
        // Speichere die Zentren C'' im Array centers[]
        for (int i = 0; i < K; i++)
        {
            centers[(L_index * 2 * K) + i + K] = m_greatest_ind[configuration.centers[i]];
            // printf("Zentrum[%d] = %s\n", (L_index * 2 * K) + i + K, pictures[centers[(L_index * 2 * K) + i + K]].name);
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
            // aus dem Clustering resultierende Distanz zum nächsten Weiteste Abstände Zentrum, conf.clusters gibt Cluster Nummer und conf.centers[] Nummer des Zentrums
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
        // prüfe die Zuordnung der Punkte anderen Punkkte neu (sind die weitesten Abstände Zentren besser?)
        for (int picture = L_index * intervall_size; picture < end_intervall; picture++) // iteriere über alle Zentren
        {
            // prüfe ob picture ein Zentrum ist oder zu den m weiteste Abstände Zentren gehört
            is_element = check_element(picture, m_greatest_ind, m);
            is_element_center = check_element(picture, centers, 2 * K * L);
            if (is_element == true || is_element_center == true)
            {
                continue;
            }
            histogram_dist(pictures[picture].name, histogram1, &w, &h, &sz, &c, &size);
            for (int center_idx = 0; center_idx < K; center_idx++) //iteriere über alle weitesten Abstand Zentren
            {
                current_center = (L_index * 2 * K) + center_idx + K;
                histogram_dist(pictures[centers[current_center]].name, histogram2, &w, &h, &sz, &c, &size);
                // Berechne Differenz der Histogramme
                difference_histograms(histogram1, histogram2, histogram_diff, &size);
                // erstelle und löse das Flussproblem
                wasser_wh(w_sz, h_sz, histogram_diff, &current_distance, &cur_time);
                if (current_distance < pictures[picture].distance) // prüfe ob gefundene Distanz geringer ist
                {
                    // Aktualisiere die nächste Distanz und das zugehörige Cluster
                    pictures[picture].distance = current_distance;
                    pictures[picture].cluster = current_center;
                    weit_dist_zug++;
                }
            }
        }
        // printf("*********************\n");
        // free allocated memory
        free(RandNum);
        free(unweighted);


    // Bestimme die Gewichte der Zentren
    calc_weights(pictures, weights, numb_pics);


    // Schreiben der Namen der Zentren und Gewichte in Files (binary und csv)
    char *out_csv = concat(output_file, ".csv");
    char *out_bin = concat(output_file, ".bin");
    chdir("..");
    FILE *zentren_csv = fopen(out_csv, "w");
    FILE *zentren_bin = fopen(out_bin, "wb");
    if (zentren_csv == NULL || zentren_bin == NULL)
    {
        printf("Fehler beim erstellen eines Files zur Ausgabe der Zentren\n");
        return 1;
    }
    int print_number = 2 * K * L;
    int name_length = strlen(pictures[0].name);
    fwrite(&print_number, sizeof(int), 1, zentren_bin); // Anzahl Elemente
    fwrite(&name_length, sizeof(int), 1, zentren_bin);  // Länge der Namen
    for (int i = 0; i < 2 * K * L; i++)
    {
        fwrite(pictures[centers[i]].name, name_length * sizeof(char), 1, zentren_bin);
        fwrite(&weights[i],  sizeof(int), 1, zentren_bin);
    }
    fclose(zentren_csv);
    fclose(zentren_bin);

    chdir(input_dir);



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

    // Close directories
    closedir(pDir);
    closedir(pDir2);
    closedir(pDir3);

    return 0;
} // Ende main

// Eingabe: Zwei Historgramme
// Ausgabe: paarweise Differenz der beiden Histogramme
void difference_histograms(double *hist1, double *hist2, double *hist_diff, int *size)
{
    double control = 0;
    for (int i = 0; i < *size; i++)
    {
        hist_diff[i] = (hist1[i] - hist2[i]);
        control += hist_diff[i];
    }
    // Falls durch Rechenungenauigkeiten der Kontrollwert kleiner als 0 ist (zu wenig vorhanden) sollen die Rollen der Bilder getauscht werden,
    // damit Überschuss vorhanden ist. Dieser kann nach Konstruktion des LPs zur Wassersteindistanz kostenfrei verschwinden
    // Wenn der Kontrollwert negativ ist, tuasche die Rollen der Bilder; DIstanzmaß ist symmetrisch
    if (control < 0)
    {
        for (int j = 0; j < *size; j++)
        {
            hist_diff[j] = -hist_diff[j];
        }
        control = -control;
    }
}

// Eingabe: leere Konfigurationsdatei
// Ausgabe: gefüllte Konfigurationsdatei
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

// Eingabe: Leere Array
// Ausgabe: Array mit Zufallszahlen aus dem Teilbereich
void get_randnumb(int RandNum[], int s, int L_index, int intervall_size)
{
    srand(time(NULL));
    //srand(1);
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


// Bestimmung der Gewichte der Cluster
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

// Updaten der Clusterzuordnung
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


// Prüft ob eine Zahl elemnt in einem Array der Größe size vorkommt
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

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}