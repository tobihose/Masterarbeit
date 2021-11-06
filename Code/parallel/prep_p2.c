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
        printf("Benutzung: ./prep_p2 <K> <L> <LL>  \n");
        return 1;
    }
    printf("*********\nVorbereitung Phase2\n*********\n");
    int K = atoi(argv[1]);
    int L = atoi(argv[2]);
    int LL = atoi(argv[3]);

    int read_length = -1;
    int read_number = -1;
    char name_buffer[50];
    int weight_buffer = -1;
    int number_bin = 2 * K * L / LL ;
    char str[12];



    FILE *f_reader = fopen("zentren.bin", "rb");
    if (f_reader  == NULL)
    {
        printf("Error! opening file");
        exit(1);
    }
    fread(&read_number, sizeof(int), 1, f_reader);
    fread(&read_length, sizeof(int), 1, f_reader);


    for (int i = 0; i < LL; i++) // iteration über die LL Bereiche
    {
        sprintf(str, "%d", i);
        char *output = concat("zentren_",str);
        char *out_bin = concat(output, ".bin");
        char *out_csv = concat(output, ".csv");
        FILE *zentren_csv = fopen(out_csv, "w"); //csv file
        FILE *zentren_bin = fopen(out_bin, "w"); // binary file
        fwrite(&number_bin, sizeof(int), 1, zentren_bin); // Anzahl Elemente
        fwrite(&read_length, sizeof(int), 1, zentren_bin);  // Länge der Namen
        for (int j = 0; j < number_bin; j++) // Iteration über die Elemente pro Bereich
        {
            fread(name_buffer, read_length * sizeof(char), 1, f_reader);
            fread(&weight_buffer,  sizeof(int), 1, f_reader);
            fwrite(&name_buffer, read_length * sizeof(char), 1, zentren_bin);
            fwrite(&weight_buffer,  sizeof(int), 1, zentren_bin);
            fprintf(zentren_csv, "%d,%s,%d\n", i, name_buffer, weight_buffer);
        }
        fclose(zentren_bin);
        fclose(zentren_csv);
    }
}
    


char *concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}
