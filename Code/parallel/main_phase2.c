#include "sample.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

int K = -1; // Anzahl der gewünschten Cluster
int L = -1;  // mögliche Intervall Größe (beispielsweise als 10K wählen -> L = n / 10K)
int LL = 1; // Anzahl Intervalle für die zweite Phase: in jedem Intervall sind dann 2 * K * L / LL Elemente; LL ist Teiler von 2KL
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
    //int K = -1;

    // zum Auslesen der Elemente eines Ordners
    struct dirent *pDirent = NULL;
    DIR *pDir = NULL;
    struct dirent *pDirent2 = NULL;
    DIR *pDir2 = NULL;
    struct dirent *pDirent3 = NULL;
    DIR *pDir3 = NULL;
    result result_algo = 0;

    // für die Zeitmessung
    double cur_time = 0;

    int w_sz = 0, h_sz = 0, size = 0, ch_sz = 0; // Breite, Höhe , Gesamtgröße und Anzahl Channels

    // Sicherstellung der korrekten Eingabe
    if (argc != 7)
    {
        printf("argc = %d\n", argc);
        printf("Benutzung: ./phase2 <Ordnername> <binary_file> <K> <L> <LL> <L_index>\n");
        return 1;
    }
    L = atoi(argv[4]);
    LL = atoi(argv[5]);
    K = atoi(argv[3]);
    int L_index = atoi(argv[6]);

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

    printf("Beginn Phase 2 für i=%d und die Parameter K: %d, L: %d, LL: %d\n",L_index, K, L, LL);

    // Speichere Eingabeordner
    char *input_dir = argv[1];
    char *binary_file = argv[2];
    //printf("binary_file: %s\n", binary_file);

    FILE *f_reader;
    if ((f_reader = fopen(binary_file, "rb")) == NULL)
    {
        printf("Error! Fehler beim Öffnen des Binary Files %s", binary_file);
        // Program exits if the file pointer returns NULL.
        exit(1);
    }

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

    // für Größe des Bildes
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

    // reserviere Speicher für Gewichte und Center der einzelnen Schritte und Gesamt unt initialisiere diese
    int intervall_size_LL = 2 * K * L / LL; // oBdA LL teil 2*K*L
    int *final_centers = (int *)malloc(K * LL * sizeof(int));
    int *final_weights = (int *)malloc(K * LL * sizeof(int));
    int *temp_centers = (int *)malloc(intervall_size_LL * sizeof(int));
    int *temp_weights = (int *)malloc(intervall_size_LL * sizeof(int));
    int *cluster_updates = (int *)malloc(2 * K * L * sizeof(int));
    for (int i = 0; i < K * LL; i++)
    {
        final_centers[i] = -1;
        final_weights[i] = 0;
    }

    //Daten auslesen
    int read_length = -1;
    int read_number = -1;
    char name_buffer[50];
    int weight_buffer = -1;
    fread(&read_number, sizeof(int), 1, f_reader);
    fread(&read_length, sizeof(int), 1, f_reader);
    char **center_names = malloc(read_number * sizeof(char *));
    for (int i = 0; i < read_number; i++)
    {
        center_names[i] = malloc(2 * (read_length) * sizeof(char));
    }
    for (int i = 0; i < read_number; i++)
    {
        fread(name_buffer, read_length * sizeof(char), 1, f_reader);
        fread(&weight_buffer, sizeof(int), 1, f_reader);
        temp_weights[i] = weight_buffer;
        strcpy(center_names[i], name_buffer);
    }

    fclose(f_reader);

    // finden der Zentrennummern
    //int centers_read[read_number];
    for (int i = 0; i < read_number; i++)
    {
        for (int j = 0; j < numb_pics; j++)
        {
            if (strcmp(center_names[i], pictures[j].name) == 0)
            {
                temp_centers[i] = j;
            }
        }
    }
    for (int i = 0; i < read_number; i++)
    {
        free(center_names[i]);
    }
    free(center_names);
    chdir(input_dir);


    // Konfigurationsdatei für den Clusteringalgorithmus für mehrere Zentren
    config multiple_centers_configuration;

    // Array zum Zählen der Elemente in jedem Cluster
    int *cluster_size = (int *)malloc(K * sizeof(int));
    for (int i = 0; i < K; i++)
    {
        cluster_size[i] = 0;
    }

    // für die zweite Phase wird eine weitere Unterteilung der Zentrenmenge (2*K*L Zentren )in LL Teilmengen durchgeführt

    // initialisiere Konfigurationsdatei
    configure(&multiple_centers_configuration, K, intervall_size_LL);


    // Jagged Array für die Distanzen innerhalb der Teilmenge der Zentren und Initilaisierung
    double *distances_centers[intervall_size_LL];
    for (int i = 0; i < intervall_size_LL; i++)
    {
        distances_centers[i] = malloc(i * sizeof(double));
    }

    // bestimme die paarweise Distanzen der gewählten Zentren
    for (int center1 = 0; center1 < intervall_size_LL; center1++)
    {
        //cur_cent1 = temp_centers[center1]; // Zentrum 1 mit dem aktuell gearbeitet wird
        // Berechne für das Intervall zu jedem Bild das Histogramm und bestimme den Wasserstein Abstand
        histogram_dist(pictures[temp_centers[center1]].name, histogram1, &w, &h, &sz, &c, &size);
        for (int center2 = 0; center2 < center1; center2++)
        {
            histogram_dist(pictures[temp_centers[center2]].name, histogram2, &w, &h, &sz, &c, &size);
            // Berechne Differenz der Histogramme
            difference_histograms(histogram1, histogram2, histogram_diff, &size);
            // erstelle und löse das Flussproblem
            wasser_wh(w_sz, h_sz, histogram_diff, &distances_centers[center1][center2], &cur_time);
        }
    }
    multiple_centers_configuration.distances = distances_centers;

    // bestimme Clustering
    build_weights(&multiple_centers_configuration, temp_weights);                              // BUILD Phase
    result_algo = algorithm(&multiple_centers_configuration, intervall_size_LL, temp_weights); // SWAP Phase
    if (result_algo == ERROR)                                                                  // Prüfe ob ein Fehler im Clustering Algorithmus aufgetreten ist
    {
        printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
        return 1;
    }

    // speichere die bestimmten Zentren als Indize von den Bildern (NICHT als Indize der Zentrenmenge centers!)
    for (int i = 0; i < K; i++)
    {
        final_centers[i] = temp_centers[multiple_centers_configuration.centers[i]];
    }
    // update cluster Zuordnung der alten Zentren als Indize der neuen Zentren
    for (int i = 0; i < intervall_size_LL; i++) //iteriere über alle bearbeiteten Zentren
    {
        //cluster_updates[temp_centers[i]] = multiple_centers_configuration.clusters[i];
    }
    // update die Gewichte
    for (int i = 0; i < intervall_size_LL; i++)
    {
        //final_weights[cluster_updates[temp_centers[i]]] += temp_weights[temp_centers[i]];
        final_weights[multiple_centers_configuration.clusters[i]] += temp_weights[i];
    }

    // Schreiben der Namen der Zentren und Gewichte in Files (binary und csv)
    chdir("..");
    char str[12];
    sprintf(str, "%d", L_index);
    char *output = concat("final_zentren", str);
    char *out_bin = concat(output, ".bin");
    char *out_csv = concat(output, ".csv");
    //printf("bin: %s, csv: %s\n", out_bin, out_csv);
    FILE *zentren_csv = fopen(out_csv, "w");
    FILE *zentren_bin = fopen(out_bin, "wb");
    if (zentren_csv == NULL || zentren_bin == NULL)
    //if (zentren_bin == NULL)
    {
        printf("Fehler beim erstellen eines Files zur Ausgabe der Zentren\n");
        return 1;
    }
    int print_number = K;
    int name_length = strlen(pictures[0].name);
    //printf("number : %d, length: %d\n", print_number, name_length);
    fwrite(&print_number, sizeof(int), 1, zentren_bin); // Anzahl Elemente
    fwrite(&name_length, sizeof(int), 1, zentren_bin);  // Länge der Namen
    fprintf(zentren_csv, "Zentrum,Name,Gewicht\n");
    for (int i = 0; i < print_number; i++)
    {
        fwrite(pictures[final_centers[i]].name, name_length * sizeof(char), 1, zentren_bin);
        fwrite(&final_weights[i], sizeof(int), 1, zentren_bin);
        fprintf(zentren_csv, "%d,%s,%d\n", i, pictures[final_centers[i]].name, final_weights[i]);
        // printf("%s,%d\n", pictures[final_centers[i]].name, final_weights[i]);
    }
    fclose(zentren_csv);
    fclose(zentren_bin);

    // free allocated memory
    free(final_centers);
    free(final_weights);
    free(temp_centers);
    free(temp_weights);
    free(cluster_updates);

    free(weights);
    free(centers);
    free(pictures);
    free(multiple_centers_configuration.centers);
    free(multiple_centers_configuration.clusters);
    free(multiple_centers_configuration.second);
    free(cluster_size);

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
    //srand(time(NULL));
    srand(1);
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

// Die Clusterzuorndung wird in ein csv file geschrieben und die Zahl der Elemente im Cluster wird gezählt
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
            //printf("pictures[%d].cluster: %d\n", j, pictures[j].cluster);
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

void check_zuordnung(picture pictures[], config *conf, int *centers, int numb_pics, int *auswertungen, int size, int w_sz, int h_sz, double *avg_time, double *total_cost, double *old_cost)
{
    double current_distance;
    double old_distance;
    int best_cluster;
    double best_distance;
    int old_cluster;
    int w, h, sz, c;
    double *histogram1 = (double *)calloc(size, sizeof(double));
    double *histogram2 = (double *)calloc(size, sizeof(double));
    double *histogram_diff = (double *)calloc(size, sizeof(double));
    double cur_time = 0;

    int test = 0;

    for (int i = 0; i < numb_pics; i++)
    {
        histogram_dist(pictures[i].name, histogram1, &w, &h, &sz, &c, &size);
        old_cluster = pictures[i].cluster;

        best_distance = INFINITY;
        for (int k_index = 0; k_index < K; k_index++)
        {
            int current_center = conf->centers[k_index];
            histogram_dist(pictures[centers[current_center]].name, histogram2, &w, &h, &sz, &c, &size);

            // Berechne Differenz der Histogramme
            difference_histograms(histogram1, histogram2, histogram_diff, &size);

            // erstelle und löse das Flussproblem
            wasser_wh(w_sz, h_sz, histogram_diff, &current_distance, &cur_time);
            //current_distance = 1;

            (*avg_time) += cur_time;
            (*auswertungen)++;

            if (old_cluster == k_index)
            {
                old_distance = current_distance;
                (*old_cost) += old_distance;
            }

            if (current_distance < best_distance)
            {
                best_distance = current_distance;
                best_cluster = k_index;
            }
        }
        if (old_distance != best_distance)
        {
            test++;
        }
        pictures[i].cluster = best_cluster;
        (*total_cost) += best_distance;
    }
    // printf("%d Bilder wurden neu zugewiesen.\n", test);
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

//
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
        sprintf(temp, "%d", i);
        mkdir(temp, 0700);
        chdir(temp);
        mkdir("sw", 0700);
        mkdir("normal", 0700);
        chdir("..");
    }
    chdir("..");
    // erstelle Ordner für die Zentren
    mkdir("./centers", 0700);
    // kopiere die Zentren in den centers und cluster Ordner, jeweils schwarz weiß und normal
    for (int i = 0; i < K; i++)
    {
        sprintf(temp_cluster3, "clusters/%d/sw", i);
        // schwarz weiße Zentren
        control += copyfile(objects[centers[final_configuration.centers[i]]].name, "centers", i, -1);
        control += copyfile(objects[centers[final_configuration.centers[i]]].name, temp_cluster3, i, -1);
        // normale Zentren
        char temp_name[strlen(objects[centers[final_configuration.centers[i]]].name)];
        int j = 0;
        for (int r = 0; r != strlen(objects[centers[final_configuration.centers[i]]].name); r++)
        {
            if (r < 2)
                continue;

            temp_name[j++] = objects[centers[final_configuration.centers[i]]].name[r];
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
        //printf("%d, %d, %d\n", length1, length2, length2- length1);
        control += copyfile(temp_name2, "centers", i, length2 - length1);
        sprintf(temp_cluster3, "clusters/%d/normal", i);
        control += copyfile(temp_name2, temp_cluster3, i, length2 - length1);
    }

    // kopiere die Bilder in die Cluster Ordner
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

int copyfile(char *infilename, char *outfileDir, int name_ergaenzt, int laenge)
{
    // name_ergaenzt gibt für die Zentren an, zu welchem Cluster diese gehören und passt den File Namen dementsprechend an
    FILE *infile; //File handles for source and destination.
    FILE *outfile;
    int ch;
    char outfilename[PATH_MAX];
    char temp[PATH_MAX];
    char temp2[PATH_MAX];

    infile = fopen(infilename, "r"); // Open the input and output files.
    if (infile == NULL)
    {
        printf("Error opening file input: %s\n", infilename);
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

    outfile = fopen(outfilename, "w");
    if (outfile == NULL)
    {
        printf("Error opening file output: %s\n", outfilename);
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

char *concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}