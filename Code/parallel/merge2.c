#include "sample.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

int main(int argc, char *argv[])
{
    // Sicherstellung der korrekten Eingabe
    if (argc != 4)
    {
        printf("Benutzung: ./merge <K> <LL> <Binary_Bennenung>\n");
        return 1;
    }
    char *filename = argv[3];
    printf("+++++++++\nBeginn zweiter Merge\n+++++++++\n");
    int K = atoi(argv[1]);
    int LL = atoi(argv[2]);
    int read_length = -1;
    int read_number = -1;
    int number_centers = K * LL;
    char name_buffer[50];
    int weight_buffer = -1;

    FILE *outfile = fopen("final_zentren.bin", "wb");
    if (outfile == NULL)
    {
        printf("Fehler beim erstellen eines Files zur Ausgabe der Zentren\n");
        return 1;
    }

    // iteriere über die L Eingabe Dateien
    for (int i = 0; i < LL; i++)
    {
        int length = snprintf(NULL, 0, "%d", i);
        char *str = malloc(length + 1);
        snprintf(str, length + 1, "%d", i);
        char *infile_pre = concat(filename, str);
        char *infile = concat(infile_pre, ".bin");
        //printf("filename: %s\n", infile);

        FILE *f_reader = fopen(infile, "rb");
        if (f_reader == NULL)
        {
            printf("Fehler beim Öffnen des Eingabefiles %s\n", infile);
            return 1;
        }
        fread(&read_number, sizeof(int), 1, f_reader);
        fread(&read_length, sizeof(int), 1, f_reader);
        if (i == 0) //schreibe Länge der Namen und Anzahl der Elemente in File
        {
            fwrite(&number_centers, sizeof(int), 1, outfile); // Anzahl Elemente
            fwrite(&read_length, sizeof(int), 1, outfile);    // Länge der Namen
        }
        for (int j = 0; j < read_number; j++)
        {
            // lesen
            fread(name_buffer, read_length * sizeof(char), 1, f_reader);
            fread(&weight_buffer, sizeof(int), 1, f_reader);

            // schreiben
            fwrite(name_buffer, read_length * sizeof(char), 1, outfile);
            fwrite(&weight_buffer,  sizeof(int), 1, outfile);
        }
    }
    fclose(outfile);

}

char *concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}
