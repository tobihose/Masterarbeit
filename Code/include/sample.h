// Header File für den Clustering Algorithmus
#ifndef algorithm_header // Prüfen ob das Header Filde bereits definiert wurde
#define algorithm_header
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern int FACTOR;

// struct für Bilder
typedef struct picture{
  char *name;               // Name des Bildes
  double distance;          // aktuelle Distanz zum nächsten Zentrum
  int cluster;              // Index des zugeörigen Clusters
} picture;

// Ausgabe für den k-medoid Algorithmus
typedef enum {
	SUCCESS,                  
	MAX_ITERATOINS,
	ERROR
} result;


typedef struct config // Konfigurationsdatei für den Algorithmus
{
    double **distances;     // Array für die Distanzen der Objekte
    int numb_objs;          // Anzahl der Objekte
    int k;                  // Anzhal der Zentren
    int max_it;             // Anzahl der maximal erlaubte Iterationen
    int tot_it;             // durchgeüfhrte Iterationen
    int *clusters;          // Array für die Indizes der Cluster der Bilder
    int *second;            // Array für die Indizes der zweitnächsten Zentren  der Bilder
    int *centers;           // Array für die Indizes der bestimmten Zentren
    double cost;            // aktueller Zielfunktionswert/Kosten
} config;

// Funktion Protoypen
void histogram_dist(char *name, double *hist, int *width, int *height, int *size, int *channels);
void wasser_wh(int w, int h, double *network, double *hist, double *time);
void difference_histograms(double *hist1, double *hist2, double *hist_diff, int *size);
void configure(config *conf, int numb_clusters, int numb_objs);
result algorithm(config *conf, int *weights);
double get_distance(int point1, int point2, config *conf);
void determine_cluster(int point, config *conf);
void print_clusters(picture pictures[], config* conf, int centers[], int numb_pics, int *cluster_size);
void get_randnumb(int RandNum[], int s, int L_index, int intervall_size);
void calc_weights(picture pictures[], int *weights, int numb_objs);
void build_weights(config *conf, int *weights);
void better_swap_weights(config *conf, int *weights);
void update_clusters(picture pictures[], config *conf, int numb_pics);
void check_zuordnung(picture pictures[], config *conf, int *centers, int numb_pics, int *auswertungen, int size, int w_sz, int h_sz, double *avg_time, double *total_cost, double *old_cost);
bool check_element(int element, int *array, int size);
int create_folders(picture objects[], int centers[], int numb_pics, config final_configuration);
int copyfile(char *infilename, char *outfileDir, int name_ergaenzt, int laenge);
int compare(const void *a, const void *b);
#endif /* algorithm_header */
