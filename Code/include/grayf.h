// Header File für den Graustufen-Algorithmus
#ifndef grayf_h // Prüfen ob das Header Filde bereits definiert wurde
#define grayf_h

extern float AKZ_WERT;
extern double MULTI;

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

// Funktion Prototypen
void RGBtoXYZ(int RGB[3], float XYZ[3]);
void XYZtoLAB(float XYZ[3], float LAB[3]);
void find_maxwhite(char *input, long *max_white, long *min_white, float *avg_white, int *numb_pics, long *numb_pixels);
int grayscale(char argv[], char *folder, int numb_pixels, long max_white);
void scale_white(long *total_white, int max_white, unsigned char *picture_pointer, int gray_channels, int gray_img_size);






#endif /* grayf */
