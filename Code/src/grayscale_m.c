#include "grayf.h"
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"
#endif

float AKZ_WERT = 0.15;       // Schwellenwert zur Konvertierung
double MULTI = 0.1;          // benutze MULTI = 0 um keine Anpassung des Weiß Wertes vorzunehmen, MULTI = 1 um alle Bilder mit Noise auf den selben Wert zu bringen
double SCALE_FACTOR = 0.7;   // Faktor für die Skalierung des Weiß Wertes, wird ausgefüht vor dem hinzufügen von Noise
double AKZ_WHITE = 500;      // Weiß Wert den ein Bild mindestens haben muss, damit es gespeichert wird; -1 um jedes Bild zu akzeptieren
double AKZ_MAX_WHITE = 100;  // Parameter zur Steuerung wie intensiv der Weiß Wert eines konvertierten Bildes maxiimal sein muss; -1 um jedes Bild zu akzepterien

/*
* Algorithmus zur Konvertierung eines Bildes in ein Graustufen-Bild
* @param input: Name der zu konvertierenden Bildes
* @param folder: Ordner des Bildes
* @param numb_pixels: Anzahl der Pixel des Bildes
* @param max_white: Maximaler Weißwert (Gewicht) aller Bilder im Ordner
* @return: in Graustufen konvertiertes Bild wird im Ordner gespeichert
*/
int grayscale(char *input, char *folder, int numb_pixels, long max_white)
{
    // Initialisierung
    float XYZ[3];       // für RGB zu XYZ
    int RGB[3];         // für RGB
    float LAB[3];       // für XYZ zu LAB

    long total_white = 0;       // gesamtes Weiß eines Bildes
    long current_white;         // Weiß des aktuell betrachteten Pixels  
    int max_white_pixel = 0;    // aktuell hellster Pixelwert

    chdir(folder);

    // das konvertierte Bild soll einen identischen Namen jeodch mit "g_" zuvor besitzen
    char *textB = "g_";
    char *textA = input;
    char *outfile = malloc(strlen(textA) + strlen(textB) + 1);
    if (outfile == NULL)
    {
        printf("Fehler beim Allokieren von Speicher für den Ausgabenamen! \n");
        exit(1);
    }
    strcpy(outfile, textB);
    strcat(outfile, textA);
    // Laden des Bildes
    int width, height, channels;
    unsigned char *img = stbi_load(input, &width, &height, &channels, 0);
    if (img == NULL)
    {
        printf("Fehler beim Laden des Bildes\n");
        exit(2);
    }
    // Bestimme Daten des Bildes
    size_t img_size = width * height * channels;           // Größe unter Beachtung der Channel
    int gray_channels = channels == 4 ? 2 : 1;             // wenn das Eingangsbild einen Alpha Channel besitzt erhält das Ausgabebild ebenfalls einen
    size_t gray_img_size = width * height * gray_channels; // Größe des Graustufen-Bildes unter Beachtung der Channel des Ausgabebildes

    if (strcmp(input, ".") != 0) // Änderung des Ordners wenn nicht der aktuelle genommen werden soll
    {
        chdir("../gray_images");
    }
    // Reserviere Speicher für das Graustufen Bild
    unsigned char *gray_img = malloc(gray_img_size);
    unsigned char *gray_img_max_white = gray_img;
    unsigned char *picture_pointer = gray_img;
    if (gray_img == NULL)
    {
        printf("Fehler beim allokieren von Speicher für das Graustufenbild\n");
        exit(3);
    }
    int magenta[3] = {255,0,255};   // Farbwerte von Magenta in RGB
    float magenta_Lab[3];           // für Magenta in Lab
    RGBtoXYZ(magenta, XYZ);         // Konvertiere Magenta in XYZ
    XYZtoLAB(XYZ, magenta_Lab);     // Konvertiere XYZ in LAB

    for (unsigned char *p = img, *pg = gray_img; p != img + img_size; p += channels, pg += gray_channels)
    {
        RGB[0] = *p, RGB[1] = *(p + 1), RGB[2] = *(p + 2); // Speichere die Farben des Pixels
        RGBtoXYZ(RGB, XYZ);                                // Konvertiere zu XYZ
        XYZtoLAB(XYZ, LAB);                                // Konvertiere zu LAB

        float deltaE = sqrt(pow(LAB[0] - magenta_Lab[0], 2) + pow(LAB[1] - magenta_Lab[1], 2) + pow(LAB[2] - magenta_Lab[2], 2));
        float perc_diff = deltaE / 296.6; // maximal mögliche Differenz zu Magenta ist 296,59
        if (perc_diff > AKZ_WERT)         // alles überhalb des akzeptierten Wertes wird schwarz
        {
            *pg = (uint8_t)0; 
        }
        else
        {
            perc_diff = perc_diff / AKZ_WERT;         // Skalierung
            *pg = (uint8_t)(255 - (perc_diff * 255)); // Weiß soll eine perfekte Übereinstimmung darstellen
            total_white += *pg;                       // Gesamt-Weiß
        }
        if ((*pg) > max_white_pixel) // neues hellstes Pixel? 
        {
            max_white_pixel = (*pg);
        }
        if (channels == 4) // falls ein Alpha Kanal vorhanden ist
        {
            *(pg + 1) = *(p + 3);
        }
    }
    // überprüfe ob genug Weiß im Bild vorhanden ist, falls nicht Verwerfen des Bildes
    if (total_white < AKZ_WHITE)
    {
        stbi_image_free(img);
        free(gray_img);
        free(outfile);
        chdir("..");
        return 1;
    }
    // überprüfe ob Pixel, die der geforderten Intensität entsprechen vorhanden sind, falls nicht Verwefe das Bild
    if (max_white_pixel < AKZ_MAX_WHITE)
    {
        stbi_image_free(img);
        free(gray_img);
        free(outfile);
        chdir("..");
        return 1;
    }
    // passe die Weiß-Werte entsprechend des Maximums an
    if (MULTI * max_white - total_white > 0)
    {
        current_white = MULTI * max_white - total_white; // Menge des Weißes, das noch vergeben werden soll
        long current_add;                                // Menge die in einem Schritt hinzugefügt wird
        long rest_size = numb_pixels;                    // restliche Anzahl an Pixel
        long total_white_new = 0;                        // neuer gesamter Weiß-Wert
        for (unsigned char *pg = gray_img_max_white; pg != gray_img_max_white + gray_img_size; pg += gray_channels)
        {
            current_add = floor(current_white / rest_size); // Menge des Weißes das hinzugefügt werden soll
            total_white_new += *pg;
            if (*pg + current_add < 255)                    // Füge Weiß hinzu, wenn 255 nicht überschritten wird
            {
                *pg += current_add;
            }
            else                                            // Setze Pixel auf 255 und passe an wieviel hinzugefügt wurde
            {
                current_add = 255 - *pg;                  
                *pg = 255;
            }
            rest_size = rest_size - 1;                      // Restliche Pixelzahl anpassen
            current_white = current_white - current_add;    // Resliches-Weiß anpassen
            total_white_new += *(pg);                       // Gesamtess-Weiß anpassen
        }
        total_white = total_white_new;
    }
    // skaliere  Intensität
    if (max_white > 0)
    {
        scale_white(&total_white, max_white, picture_pointer, gray_channels, gray_img_size);
    }
    stbi_write_jpg(outfile, width, height, gray_channels, gray_img, 100); // Speichere das Graustufenbild
    chdir("..");
    // Speicher freigeben
    stbi_image_free(img);
    free(gray_img);
    free(outfile);
    return 0;
}

/*
* Algorithmus zur Skalierung der Weiß-Werte (Gewichte)
* @param total_white: Gesamtes Weiß (Gewicht) des aktuellen Bildes vor Skalierung
* @param max_white: Maximaler Weißwert (Gewicht) aller Bilder im Ordner
* @param picture_pointer: Pointer zum ersten Pixel des Grautufen-Bildes
* @param gray_channels: Anzahl der Kanäle (1 oder 2) des Graustufen-Bildes
* @param gray_img_size: Maximaler Weißwert (Gewicht) aller Bilder im Ordner
* @return: skalierte Pixel um SCALE_FACTOR des Maximalen-Weiß (Gewicht) zu erhalten.
*/
void scale_white(long *total_white, int max_white, unsigned char *picture_pointer, int gray_channels, int gray_img_size)
{
    int new_total_white = 0;        // neues Gesamtes Weiß
    double rundungs_weiß = 0.0;     // überschüssiges Weiß entstanden durch Abrunden
    double factor;                  // Saklierungsfaktor
    double temp;                    // Temporäer Weiß-Wert
    // Der Skalierungsfaktor ist ein Wert größer 0 und gibt an mit welchem Wert 1+factor mutipliziert weren muss um das korrekte Weiß Level zu erhalten
    if (*total_white > 0)
    {
        factor = 1.0 * max_white / *total_white - 1.0;
    }
    else
    {
        factor = 0;
        return;
    }
    for (unsigned char *pg = picture_pointer; pg != picture_pointer + gray_img_size; pg += gray_channels)
    {
        temp = (*pg) * factor * SCALE_FACTOR;                     // hinzuzufügender Wert
        if ((*pg) + temp + floor(rundungs_weiß) < 255)            // prüfen ob Pixel immernoch weniger als 255 Weiß-Wert hat
        {
            *pg = (*pg) + floor(temp) + floor(rundungs_weiß);     // skaliere mit dem Faktor, abgerundet da integer, und füge übriges weiß von vorher hinzu
            rundungs_weiß = rundungs_weiß - floor(rundungs_weiß); // hinzugefügtes Rundungsweiß anpassen
            rundungs_weiß += temp - floor(temp);                  // es kann immer nur eine Ganzzahl hinzugefügt werden, Überschuss ist rundungs_weiß
        }
        else
        {
            rundungs_weiß += (*pg) + temp - 255;                  // füge überschüssiges rundungs_weiß hinzu
            *pg = 255;                                            // setze Pixel auf 255
        }

        new_total_white += *(pg);                                 // passe Gesamtes Weiß an
    }
    *total_white = new_total_white;                               // gebe Gesamtes Weiß aus
}

/*
* Algorithmus zur Konvertierung eines Pixels aus dem RGB-Farbraum in den XYZ-Farbraum
* @param RGB: Farbwerte im RGB-Farbraum (Array)
* @param XYZ: Farbwerte im XYZ-Farbraum (leerer Array)
* @return: konvertierter Pixel im Array XYZ
*/
void RGBtoXYZ(int RGB[3], float XYZ[3])
{
    float var_R = RGB[0] / 255.0;
    float var_G = RGB[1] / 255.0;
    float var_B = RGB[2] / 255.0;

    if (var_R > 0.04045)
    {
        var_R = pow((var_R + 0.055) / 1.055, 2.4);
    }
    else
    {
        var_R = var_R / 12.92;
    }

    if (var_G > 0.04045)
    {
        var_G = pow((var_G + 0.055) / 1.055, 2.4);
    }
    else
    {
        var_G = var_G / 12.92;
    }

    if (var_B > 0.04045)
    {
        var_B = pow((var_B + 0.055) / 1.055, 2.4);
    }
    else
    {
        var_B = var_B / 12.92;
    }
    var_R = 100 * var_R;
    var_G = 100 * var_G;
    var_B = 100 * var_B;
    //Observer. = 2°, Illuminant = D65
    XYZ[0] = var_R * 0.4124 + var_G * 0.3576 + var_B * 0.1805;
    XYZ[1] = var_R * 0.2126 + var_G * 0.7152 + var_B * 0.0722;
    XYZ[2] = var_R * 0.0193 + var_G * 0.1192 + var_B * 0.9505;
}

/*
* Algorithmus zur Konvertierung eines Pixels aus dem XYZ-Farbraum in den LAB-Farbraum
* @param XYZ: Farbwerte im XYZ-Farbraum (Array)
* @param LAB: Farbwerte im LAB-Farbraum (leerer Array)
* @return: konvertierter Pixel im Array LAB
*/
void XYZtoLAB(float XYZ[3], float LAB[3])
{   // Es wird eine Standard "D65" 6500 Kelvin Lichtquelle benutzt
    // https://en.wikipedia.org/wiki/Illuminant_D65
    float D65[3] = {95.047, 100, 108.883};
    float var_X = XYZ[0] / D65[0];
    float var_Y = XYZ[1] / D65[1];
    float var_Z = XYZ[2] / D65[2];
    if (var_X > 0.008856)
    {
        var_X = pow(var_X, 0.3333333333);
    }
    else
    {
        var_X = (7.787 * var_X) + (16 / 116);
    }
    if (var_Y > 0.008856)
    {
        var_Y = pow(var_Y, 0.3333333333);
    }
    else
    {
        var_Y = (7.787 * var_Y) + (16 / 116);
    }
    if (var_Z > 0.008856)
    {
        var_Z = pow(var_Z, 0.3333333333);
    }
    else
    {
        var_Z = (7.787 * var_Z) + (16 / 116);
    }
    LAB[0] = (116 * var_Y) - 16;
    LAB[1] = 500 * (var_X - var_Y);
    LAB[2] = 200 * (var_Y - var_Z);
}

/*
* Algorithmus zur Bestimmung des Maximalen-Weiß-Wertes (Gewicht) eines Ordners von Bildern
* @param input: Ordnername
* @param max_white: für maximalen Weiß-Wert (leer)
* @param min_white: für minimalen Weiß-Wert (leer)
* @param avg_white: für mittleren Weiß-Wert (leer)
* @param numb_pics: Anzahl der Bilder im Ordner
* @param numb_pixels: Größe der Bilder
* @return: Maximaler-Weiß-Wert, Minimaler-Weiß-Wert, Mittlerer-Weiß-Wert
*/
void find_maxwhite(char *input, long *max_white, long *min_white, float *avg_white, int *numb_pics, long *numb_pixels)
{
    // Initialisierung
    float XYZ[3], LAB[3], magenta_Lab[3];
    int RGB[3];
    long total_white;
    int point, width, height, channels;   // Daten eines Bildes
    DIR *pDir;                         
    struct dirent *pDirent;
    size_t img_size;                      // Größe des Bildes
    int magenta[3] = {255,0,255};         // RGB Magenta Farbwerte
    RGBtoXYZ(magenta, XYZ);               // konvertiere Magenta in XYZ-Farbraum
    XYZtoLAB(XYZ, magenta_Lab);           // konvertiere Magenta in LAB-Farbraum
    pDir = opendir(input);

    if (pDir == NULL)
    {
        printf("Ordner '%s' kann nicht geöffnet werden,\n", input);
        return;
    }
    if (strcmp(input, ".") != 0)
    {
        chdir(input);
    }
    while ((pDirent = readdir(pDir)) != NULL) // iteriere über alle Bilder im Ordner
    {
        char *test = pDirent->d_name;
        if (strstr(test, ".png") != NULL)     // überprüfe ob es sich um ein png Bild handelt
        {
            total_white = 0;                  // Initialisierung Gesamt-Weiß (Gewicht)
            unsigned char *img = stbi_load(pDirent->d_name, &width, &height, &channels, 0);
            if (img == NULL)
            {
                printf("Fehler beiim Laden des Bildes\n");
                exit(2);
            }
            img_size = width * height * channels;
            chdir(input);
            (*numb_pics)++;
            for (unsigned char *p = img; p != img + img_size; p += channels) // Iteration über alle Pixel
            {
                RGB[0] = *p;
                RGB[1] = *(p + 1);
                RGB[2] = *(p + 2);
                RGBtoXYZ(RGB, XYZ); // Konvertierung des Pixels in XYZ-Farbraum
                XYZtoLAB(XYZ, LAB); // Konvertierung des Pixels in LAB-Farbraum
                float deltaE = sqrt(pow(LAB[0] - magenta_Lab[0], 2) + pow(LAB[1] - magenta_Lab[1], 2) + pow(LAB[2] - magenta_Lab[2], 2));
                float perc_diff = deltaE / 296.6; // maximal mögliche Differenz zu Magenta ist 296,59
                if (perc_diff > AKZ_WERT)         // alles überhalb des akzeptierten Wertes wird schwarz
                {
                    point = 0; 
                }
                else
                {
                    perc_diff = perc_diff / AKZ_WERT;           // Skalierung
                    point = (uint8_t)(255 - (perc_diff * 255)); // Weiß soll eine perfekte Übereinstimmung darstellen
                }
                total_white += point;                           // Gesamt-Weiß
            }
            if (total_white > *max_white)       // neuer höchster Weiß-Wert (Gewicht)?
            {
                *max_white = total_white;
            }
            else if (total_white < *min_white) // neuer tiefster Weiß-Wert (Gewicht)?
            {
                *min_white = total_white;
            }
            *avg_white += total_white;         // durschnittlicher Weiß-Wert (Gewicht)
        }
    }
    closedir(pDir);
    if (strcmp(input, ".") != 0)
    {
        chdir("..");
    }
    *numb_pixels = width * height;
}