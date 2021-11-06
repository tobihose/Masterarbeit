#include "sample.h"
#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"
int FACTOR = 1000000.0; // Skalierung um Wasserstein zu beschleunigen, durch ausprobieren bestimmt; keinen Einfluss auf Algorithmus

// diese Funktion bestimmt ausgehend von einem Graustufen Bild die Histogramm Werte und gibt diese in einem Array zurück
void histogram_dist(char *name, double *hist, int *width, int *height, int *size, int *channels, int *buckets)
{
    // Initialisiere Summe der Weißwerte
    unsigned long sum_white = 0;
    // Laden des Bildes
    unsigned char *img = stbi_load(name, width, height, channels, 0);
    if (img == NULL)
    {
        printf("Error in loading the image \"%s\"\n", name);
        exit(1);
    }
    *size = *width * *height;                // Größe des Bildes
    unsigned char *p = img, *pp = img;       // Pointer auf Beginn des Bildes
    // Itereire über die Pixel des Bildes
    for (int i = 0; i < *size; i++) 
    {
            sum_white += *p;                 // erhöhe Weiß-Summe
            p = p + *channels;               // gehe channels Schritte weiter für den nächsten Punkt
    }

    // Itereire über die Pixel des Bildes
    for (int i = 0; i < *size; i++)
    {
            hist[i]= *pp * FACTOR / sum_white; // bestimme Histogrammwerte
            pp = pp + *channels;               // gehe channels Schritte weiter für den nächsten Punkt
    }
    // Speicher freigeben
    stbi_image_free(img);
}
