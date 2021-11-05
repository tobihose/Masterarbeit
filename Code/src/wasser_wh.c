// aufbauend auf https://www.gurobi.com/wp-content/plugins/hd_documentations/content/pdf/quickstart_linux_8.1.pdf
#include "gurobi_c.h"
#include "sample.h"

/*
* Algorithmus zur Bestimmung der Wasserstein-Distanz zwischen zwei Bildern
* @param w: Breite der Bilder
* @param h: Höhe der Bilder
* @param network: die Knoten mit den Exzessen als Array
* @param dist: Parameter zur Übergabe der bestimmten Distanz
* @param time: Parameter zur Übergabe der Laufzeit
* @return: Wasserstein-Distanz gespeichert in dem Parameter dist
*/
void wasser_wh(int w, int h, double *network, double *dist, double *time)
{
  // Initialisierung 
  clock_t start, end;                                                 // Zeitmessung
  int flows = 4 * w * h - 2 * w - 2 * h;                              // Gesamtanzahl an Flüssen -> Anzahl der Terme in Zielfunktion
  int size = w * h;                                                   // Größe des Bildes -> Anzahl der Knoten -> Anzahl Nebenbedingungen
  int horizontal = h * (w - 1) * 2;                                   // Anzahl der horizontalen Flüsse
  GRBenv *env = NULL;                                                 // Umgebung für Gurobi
  GRBmodel *model = NULL;                                             // Modell für Gurobi
  int error = 0;                                                      // zum Fehler auffangen
  double *sol = (double *)malloc((flows + size) * sizeof(double));    // für die Lösung
  int *ind = (int *)malloc(10 * sizeof(int));                         // Indizes der Zielfunktionsparameter (Array)
  double *val = (double *)malloc(10 * sizeof(double));                // Konstanten für Nebenbedingugnen (Array)
  double *obj = (double *)malloc((flows + size) * sizeof(double));    // Konstanten für Nebenbedingugnen Zielfunktion (Array)
  int optimstatus;                                                    // Status zur Bestimmung der optimalen Lösung
  double objval;                                                      // Zielfunktionswert
  int r_mod_w;                                                        // r modulo w
  int r_floor_w;                                                      // r durch w abgerundet
  int index1, index2, index3, index4, index5, index6, index7, index8; // Indizes für die Nebenbedingungen

  for (int i = 0; i < 10; i++)
  {
    val[i] = 1;
    ind[i] = 0;
  }

  // Erstellung der Umgebung; Prüfen auf Fehler
  error = GRBemptyenv(&env);
  if (error)
    goto QUIT;

  // Einstellung für Output des Gurobi Optimierers: 0 kein Output, 1 Output; Prüfen auf Fehler
  error = GRBsetintparam(env, GRB_INT_PAR_OUTPUTFLAG, 0);
  if (error)
    goto QUIT;

  // unter 40.000 Nebenbedingungen funktioniert der Simplex am Besten (Mehtode 0)
  // über 40.000 Nebenbedingungen funktioniert der Barrier am Besten (Methode 2)
  if (size < 40000)
  {
    error = GRBsetintparam(env, GRB_INT_PAR_METHOD, 0);
    if (error)
      goto QUIT;
  }
  else
  {
    error = GRBsetintparam(env, GRB_INT_PAR_METHOD, 2);
    if (error)
      goto QUIT;
  }

  // Wahl der Anzahl Threads für den Gurobi Optimierer; Prüfen auf Fehler
  error = GRBsetintparam(env, GRB_INT_PAR_THREADS, 1);
  if (error)
    goto QUIT;

  // Erstellung einer leeren Umgebung; Prüfen auf Fehler
  error = GRBstartenv(env);
  if (error)
    goto QUIT;

  // Erstellung eines leeren Modells
  error = GRBnewmodel(env, &model, "model", 0, NULL, NULL, NULL, NULL, NULL);
  if (error)
    goto QUIT;

  // Füllen des Arrays für die Zielfunktion, alles hat die selben Kosten
  for (int i = 0; i < flows; i++)
  {
    obj[i] = 1;
  }
  //  Flüsse für möglichen Überschuss mit Kosten 0 hinzufügen 
  for (int i = flows; i < flows + size; i++)
  {
    obj[i] = 0;
  }
  // Die Parameter werden dem Modell übergeben, alle Flüsse sind stetig; Prüfen auf Fehler
  error = GRBaddvars(model, flows + size, 0, NULL, NULL, NULL, obj, NULL, NULL, NULL,
                     NULL);
  if (error)
    goto QUIT;

  // Folgend werden die Nebenbedingungen erstellt und dem Modell übergeben
  for (int r = 0; r < size; r++) // iteriere über die Pixel (Histogrammwerete)
  {
    int variable_index = 0; // zur korrekten Befüllung der Arrays ind[] und val[]

    // kostenlos zu Senke führender Fluss für jeden Knoten
    ind[variable_index] = r + flows;
    val[variable_index] = 1; 
    variable_index++;

    // Bestimmung von Werten zur Nummerierung
    r_mod_w = r % w;
    r_floor_w = floor(r / w);
    index1 = r_mod_w * 2 + r_floor_w * 2 * (w - 1) - 2;
    index2 = index1 + 1;
    index3 = index1 + 2;
    index4 = index1 + 3;

    index5 = r_mod_w * 2 + horizontal + r_floor_w * 2 * w;
    index6 = index5  +1;
    index7 = index5 - 2 * w;
    index8 = index5 - 2 * w + 1;

    // horizontale Flüsse
    if (r_mod_w == 0) // linker Rand
    {
      ind[variable_index] = index3;
      val[variable_index] = 1;
      variable_index++;
      ind[variable_index] = index4;
      val[variable_index] = -1;
      variable_index++;
    }
    else if (r_mod_w == (w - 1)) // rechter Rand
    {
      ind[variable_index] = index1;
      val[variable_index] = -1;
      variable_index++;
      ind[variable_index] = index2;
      val[variable_index] = 1;
      variable_index++;
    }
    else // Rest
    {
      ind[variable_index] = index3;
      val[variable_index] = 1;
      variable_index++;
      ind[variable_index] = index4;
      val[variable_index] = -1;
      variable_index++;
      ind[variable_index] = index1;
      val[variable_index] = -1;
      variable_index++;
      ind[variable_index] = index2;
      val[variable_index] = 1;
      variable_index++;
    }

    // veritkale Flüsse
    if (r_floor_w == 0) // erste Reihe
    {
      ind[variable_index] = index5;
      val[variable_index] = 1;
      variable_index++;
      ind[variable_index] = index6;
      val[variable_index] = -1;
      variable_index++;
    }
    else if (r_floor_w == (h - 1)) // letzte Reihe
    {
      ind[variable_index] = index7;
      val[variable_index] = -1;
      variable_index++;
      ind[variable_index] = index8;
      val[variable_index] = 1;
      variable_index++;
    }
    else // Rest
    {
      ind[variable_index] = index5;
      val[variable_index] = 1;
      variable_index++;
      ind[variable_index] = index6;
      val[variable_index] = -1;
      variable_index++;
      ind[variable_index] = index7;
      val[variable_index] = -1;
      variable_index++;
      ind[variable_index] = index8;
      val[variable_index] = 1;
      variable_index++;
    }

    // hinzufügen der Nebenbedingung zum Modell
    error = GRBaddconstr(model, variable_index, ind, val, GRB_EQUAL, network[r], NULL);
    if (error)
      goto QUIT;
    
  }

  start = clock();
  // Optimierung des Modells
  error = GRBoptimize(model);
  if (error)
    goto QUIT;

  end = clock();
  *time = ((double)(end - start)) / CLOCKS_PER_SEC;


  // Speicherung von Informationen zur Lösung; Prüfung auf Fehler
  error = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &optimstatus);
  if (error)
    goto QUIT;
  error = GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, &objval);
  if (error)
    goto QUIT;
  error = GRBgetdblattrarray(model, GRB_DBL_ATTR_X, 0, flows + size, sol);
  if (error)
    goto QUIT;

  // Ausgabe entsprechend zur Art der Lösung
  if (optimstatus == GRB_OPTIMAL)
  {
    *dist = objval;
  }
  else if (optimstatus == GRB_INF_OR_UNBD)
  {
    printf("Modell ist unbeschränkt oder ungültig\n");
  }
  else
  {
    printf("Optimierung frühzeitig gestoppt\n");
  }

// verschiedene Fehlermeldungen
QUIT:
  if (error)
  {
    // Fehler ausgeben und Programm beenden
    printf("ERROR: %s\n", GRBgeterrormsg(env));
    exit(1);
  }

  // Allokierten Speicher freigeben
  GRBfreemodel(model);
  GRBfreeenv(env);
  free(ind);
  free(obj);
  free(val);
  free(sol);
}
