#include "grayf.h"
float AKZ_WERT;

/*
* Algorithmus zur Konvertierung eines Ordners von png Bildern in Graustufen-Bilder
*/
int main(int argc, char *argv[])
{
    // Initialisierung
    struct dirent *pDirent; // Zum Lesen des Ordners
    DIR *pDir;                         // zum Öffnes des Ordners
    long max_white = 0;                // Maximales Weiß
    int numb_pics = 0;                 // Anzahl Bilder
    long min_white = INFINITY;         // Minimales Weiß
    float avg_white = 0;               // Durchschnittliches Weiß
    long numb_pixels;                  // Anzahl Pixel pro Bild
    int not_saved_pics = 0;            // Anzahl nicht gespeicherter Bilder

    // Eingabe prüfen
    if (argc != 2)
    {
        printf("./gray_folder <Ordnername> \n");
        return 1;
    }
    // Name des Eingabeordners speichern
    char *input_dir = argv[1];

    // Prüfen ob der Ordner geöffnet werden kann
    pDir = opendir(input_dir);
    if (pDir == NULL)
    {
        printf("Der Ordner kann nicht geöffnet werden '%s'\n", argv[1]);
        return 1;
    }

    // Bestimung des maximalen Weiß Wertes
    find_maxwhite(input_dir, &max_white, &min_white, &avg_white, &numb_pics, &numb_pixels);

    if (max_white == 0)
    {
        printf("Passe Parameter an, alle Bilder sind schwarz\n");
        return 1;
    }
    printf("max_white: %ld, min_white: %ld, avg_white: %f, numb_pixels: %ld\n", max_white, min_white, avg_white / numb_pics, numb_pixels);

    // konvertiere die Bilder, iteriere hierzu über alle Elemente des Ordners
    while ((pDirent = readdir(pDir)) != NULL)
    {
        char *pic = pDirent->d_name;
        if (strstr(pic, ".png") != NULL) // zu konvertierende Bilder sind vom Format "png"
        {
            not_saved_pics += grayscale(pDirent->d_name, input_dir, numb_pixels, max_white); // Bild konvertieren und Zählen der nicht konvertierten Bilder
        }
    }
    printf("%d von %d Bildern wurden konvertiert\n", numb_pics - not_saved_pics, numb_pics);
    
    closedir(pDir); // Ordner schließen
    return 0;       // Programm beenden
}
