#include "sample.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

int K;  // Anzahl der gewünschten Cluster
int L;  // mögliche Intervall Größe (beispielsweise als 10K wählen -> L = n / 10K)
int LL; // Anzahl Intervalle für die zweite Phase: in jedem Intervall sind dann 2 * K * L / LL Elemente; LL ist Teiler von 2KL
            // wähle LL > L^2k/n um für den zweiten Teil weniger als 2 nk Berechnungen zu brauchen

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
    clock_t start2 = 0, end2 = 0, start_BUILD = 0, start_PAM = 0, end_PAM = 0, end_BUILD = 0;
    double cpu_time_used2 = 0, time_BUILD = 0, time_PAM = 0;
    double cur_time = 0, max_time = 0, min_time = INFINITY, avg_time = 0;

    // für die Anzahl Auswertungen des Distanzmaßes
    int auswertungen = 0;


    // für die Kosten
    double total_cost = 0;
    double old_cost = 0;

    int w_sz = 0, h_sz = 0, size = 0, ch_sz = 0; // Breite, Höhe , Gesamtgröße und Anzahl Channels

    // Sicherstellung der korrekten Eingabe
    if (argc != 6)
    {
        printf("Benutzung: ./phase3 <Ordnername> <binary_file> <K> <L> <LL>\n");
        return 1;
    }
    L = atoi(argv[4]);
    K = atoi(argv[3]);
    LL = atoi(argv[5]);

    printf("***********\nBeginn Phase 3\n***********\n");



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

    // printf("K: %d, L: %d, LL: %d\n", K, L, LL);

    // Speichere Eingabeordner
    char *input_dir = argv[1];
    char *binary_file = argv[2];
    // printf("binary_file: %s\n", binary_file);

    FILE *f_reader;
    if ((f_reader = fopen(binary_file, "rb")) == NULL)
    {
        printf("Error! Fehler beim Öffnen des Binary Files");
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
    // printf("Es sind %i Elemente im Ordner.\n", numb_pics);

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

    start2 = clock();

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
    int *final_centers = (int *)malloc(K * LL * sizeof(int));
    int *final_weights = (int *)malloc(K * LL * sizeof(int));
    for (int i = 0; i < K * LL; i++)
    {
        final_centers[i] = -1;
        final_weights[i] = 0;
    }



    //  Daten auszulesen
    int read_length = -1;
    int read_number = -1;
    char name_buffer[50];
    int weight_buffer = -1;
    fread(&read_number, sizeof(int), 1, f_reader);
    fread(&read_length, sizeof(int), 1, f_reader);
    // printf("number: %d, length: %d\n", read_number, read_length);
    char **center_names = malloc(read_number * sizeof(char *));
    for (int i = 0; i < read_number; i++)
    {
        center_names[i] = malloc((read_length + 1) * sizeof(char));
    }
    for (int i = 0; i <  read_number; i++)
    {
        fread(name_buffer, read_length * sizeof(char), 1, f_reader);
        fread(&weight_buffer,  sizeof(int), 1, f_reader);
        final_weights[i]= weight_buffer;
        strcpy(center_names[i],name_buffer);
    } 
    fclose(f_reader);


    // finden der Zentrennummern
    for(int i = 0; i < read_number; i++)
    {
        for (int j = 0; j < numb_pics; j++)
        {
            if(strcmp(center_names[i], pictures[j].name) == 0)
            {
                final_centers[i] = j;
            }
        }
    }
    for(int i = 0; i < read_number; i++)
    {
        free(center_names[i]);
    }
    free(center_names);
    chdir(input_dir);


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
        histogram_dist(pictures[final_centers[center1]].name, histogram1, &w, &h, &sz, &c, &size);
        for (int center2 = 0; center2 < center1; center2++)
        {
            histogram_dist(pictures[final_centers[center2]].name, histogram2, &w, &h, &sz, &c, &size);
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
    build_weights(&final_configuration, final_weights); // BUUILD Phase (mit Gewichten)
    end_BUILD = clock();
    start_PAM = clock();
    result_algo = algorithm(&final_configuration, K * LL, final_weights); // SWAP Phase (mit Gewichten)
    end_PAM = clock();
    time_PAM += ((double)(end_PAM - start_PAM)) / CLOCKS_PER_SEC;
    time_BUILD += ((double)(end_BUILD - start_BUILD)) / CLOCKS_PER_SEC;
    if (result_algo == ERROR) // Prüfe auf einen Fehler
    {
        printf("Ein Fehler im kmedoids Algorithmus ist aufgetreten!\n");
        return 1;
    }



    // Weise die Punkte ihrem nächsten Zentrum zu (n*k Auswertungen)
    check_zuordnung(pictures, &final_configuration, final_centers, numb_pics, &auswertungen, size, w_sz, h_sz, &avg_time, &total_cost, &old_cost);


    printf("Die finalen Zentren sind:\n");
    for (int i = 0; i < K; i++)
    {
        printf("Zentrum %d: Bild Nummer %d, Name %s\n", i, final_centers[final_configuration.centers[i]], pictures[final_centers[final_configuration.centers[i]]].name);
    }
    printf("*********************\n");

    // Sortiere die Bilder in die zugehörigen Ordner
    create_folders(pictures, final_centers, numb_pics, final_configuration);

    // free allocated memory
    free(final_centers);
    free(final_weights);

    free(final_configuration.centers);
    free(final_configuration.clusters);
    free(final_configuration.second);

    for (int i = 0; i < K * LL; i++)
    {
        free(final_distances[i]);
    }
    end2 = clock();
    cpu_time_used2 += ((double)(end2 - start2)) / CLOCKS_PER_SEC;

    printf("*********\nPHASE 3 ABGESCHLOSSEN\n*********\n");

    free(centers);
    free(pictures);

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
    printf("%d Bilder wurden neu zugewiesen.\n", test);
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
