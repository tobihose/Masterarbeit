// 
//
//
//
//
#include "sample.h"

float EPSILON = 0.9999;

#define CENTROID(a) (conf->centers[conf->clusters[a]]) 
#define SECOND(a) (conf->centers[conf->second[a]])

result algorithm(config *conf, int intervall_size, int *weights)
{
    // Initialisierung der Iterationszahl, alte Kosten
    int iterations = 0;
    double old_costs = conf->cost;

    // Corner Case: wenn nur ein Cluster gefunden werden soll liefert Build bereits die optimale Lösung (Greedy)
    if (conf->k == 1)
    {
        conf->tot_it = 0;
        return SUCCESS;
    }
    // läuft bis while Schleife wegen zu vieler Iterationen oder Konvergenz beendet wird
    while (1)
    {
        // bestimme den besten Tausch und führe ihn aus falss sich die Kosten verbessern
        better_swap_weights(conf, weights);

        // Konvergenzprüfung, falls kein geeigneter Tausch gefunden wurde beende den Algorithmus
        if (conf->cost >= EPSILON * old_costs)
        {
            conf->tot_it = iterations;
            //printf("Kosten: %f\n", conf->cost);
            return SUCCESS;
        }

        // Inkremetierung vor erneutem durchlaufen der Schleife
        iterations++;

        // speichere  Kosten um Konvergenz für die nächste Iteration zu prüfen und auf dieses später zuzugreifen
        old_costs = conf->cost;

        // Prüfe ob die maximale Iterationszahl überschritten wurde
        if (iterations > conf->max_it)
        {
            conf->tot_it = iterations;
            return MAX_ITERATOINS;
        }

    }
}



// Eingabe: Indizes zweier Punkte, Cluster Konfigurationsdatei (Abstände)
// Ausgabe: Abstand der beiden Punkte
double get_distance(int point1, int point2, config *conf)
{
    // die Abstände sind als untere Dreiecksmatrix gespeichert
    // der Abstand eines Punktes zu sich selber ist immer 0
    double distance;
    if (point1 > point2) 
    {
        distance = conf->distances[point1][point2];
        return distance;
    }
    else if (point1 < point2)
    {
        distance = conf->distances[point2][point1];
        return distance;
    }
    else  //  (point1 == point2)
    {
        distance = 0;
        return distance;
    }
}

// Eingabe: Index eines Punktes und Konfigurationsdatei
// Ausgabe: Zuordnung des Punktes zu seinem nächsten und zweitnächsten Pubkt, zugehörige Distanz
void determine_cluster(int point, config *conf)
{
    // Initialisierung
    double cur_distance;
    double best_distance = INFINITY;
    double best_second_distance = INFINITY;
    // iteriere über alle Zentren
    for (int medoid = 0; medoid < conf->k; medoid++)
    {
        cur_distance = get_distance(point, conf->centers[medoid], conf); // aktuelle Distanz
        if (cur_distance < best_distance)
        {
            if (medoid > 0) // edga case für erste Iteration, für die erste Iteration wird das zweitnächste Cluster nicht gesetzt
            {
                // falls neue beste Zuordnung gefunden wird die vorherige Zuordnung zur zweitnächste
                conf->second[point] = conf->clusters[point];
                best_second_distance = best_distance;
            }
            // setze die Zuordnung und Distanz neu
            conf->clusters[point] = medoid;
            best_distance = cur_distance;
        }
        // ist die aktuelle Distanz kleiner als die bislang zweitbeste?
        else if (cur_distance < best_second_distance)
        {
            // aktualisiere die zweitnächste Distanz und das zweitnächste Zentrum
            conf->second[point] = medoid;
            best_second_distance = cur_distance;
        }
    }
}


// Eingabe: Cluster Konfigurationsdatei und Gewichte der Punkte
// Ausgabe: Clusterzentren, die mit Greedy bestimmt wurden und in der Konfigurationsdatei gespeichert sind
void build_weights(config *conf, int *weights)
{
    // Initialisierueng
    double best_dist = INFINITY;                // beste Distanz
    bool member;                                // zur Prüfung ob es sich bereits um ein Zetrum handelt
    double delta;                               // für Änderung der Kosten eines Punktes
    double zent_dist;                           // Abstand zum Zentrum
    double gain;                                // Verbesserung der Lösung
    double best_gain = INFINITY;                // beste Verbesserung der Lösung
    double curbest_second_distance = INFINITY;
    double cur_distance;

    // Bestimmung des ersten Zentrums
    for (int center = 0; center < conf->numb_objs; center++) // iteriere über alle Punkte als Zentrum
    {
        conf->cost = 0; // die Kosten sind zunächst 0
        for (int r = 0; r < conf->numb_objs; r++) // iteriere über alle Punkte
        {
            conf->cost += weights[r] * get_distance(r, center, conf); // füge den gewichteten Abstand den Kosten hinzu
        }

        // neues bestes erstes Zentrum gefunden?
        if (conf->cost < best_dist)
        {
            // speichere Zentrum
            conf->centers[0] = center;
            best_dist = conf->cost;
        }
    }
    // setzte die Kosten auf die Besten gefundenen Kosten
    conf->cost = best_dist;

    // weise alle Punkte dem ersten Zentrum zu
    for (int i = 0; i < conf->numb_objs; i++)
    {
        conf->clusters[i] = 0;
    }

    // bestimme die anderen Zentren
    for (int k = 1; k < conf->k; k++)
    {
        best_gain = INFINITY; // Initialisiere für jedes mögliche Zentrum die Beste Änderung der Kosten mit Unendlich
        for (int i = 0; i < conf->numb_objs; i++)
        {
            gain = 0; // für jede mögliche Zentrum ist die Änderung der Kosten zunächst
            for (int j = 0; j < conf->numb_objs; j++) //  iterie über alle anderen Punkte
            {
                member = false; 
                // prüfe ob i berreits in Zentrenmenge ist
                for (int check_k = 0; check_k < k; check_k++)
                {
                    if (conf->centers[check_k] == i)
                    {
                        member = true;
                    }
                }
                if (member == false) // Wenn i bereits ein Zentrum ist muss kein Gain berechnet werden, dieser ist 0
                {
                    zent_dist = get_distance(CENTROID(j), j, conf); // Abstand zum nächsten Zentrum
                    delta = weights[j] * (get_distance(i, j, conf) - zent_dist); // Änderung der Kosten für den Punkt
                    // Falls Verbesserung entseht, verringere Kosten
                    if (delta < 0)
                    {
                        gain += delta;
                    }
                }
            }

            // falls aktuell bestes Zentrum Update
            if (gain < best_gain)
            {
                best_gain = gain;
                conf->centers[k] = i;
            }
        }
        conf->cost += best_gain; // Updaten der Kosten

        // weise Punkte dem jeweils besten Zentren zu
        for (int i = 0; i < conf->numb_objs; i++)
        {
            best_dist = INFINITY;
            curbest_second_distance = INFINITY;
            for (int center = 0; center <= k; center++)
            {
                cur_distance = get_distance(conf->centers[center], i, conf);

                // alt
                // neue beste Zuordnung gefunden?
                // if (cur_distance < best_dist)
                // {
                //     conf->clusters[i] = center;
                //     best_dist = cur_distance;
                // }
                // neu

                // neue beste Zuordnung gefunden?
                if (cur_distance < best_dist)
                {
                    if (center > 0) // edga case für erste Iteration
                    {
                        conf->second[i] = conf->clusters[i];
                        curbest_second_distance = best_dist;
                    }
                    conf->clusters[i] = center;
                    best_dist = cur_distance;
                }
                else if (cur_distance < curbest_second_distance)
                {
                    conf->second[i] = center;
                    curbest_second_distance = cur_distance;
                }
            }
        }
    }
}

void better_swap_weights(config *conf, int *weights)
{
    // Initialisierung
    double best_td = INFINITY;                          // aktuell beste Verbesserung der Kosten
    double *td = malloc(conf->k * sizeof(double));      // Array für die Verbesserungen der Kosten
    double d_oj;                                        // Abstand vom Punkt x_o zum Pubkt x_j
    int best_medoid;                                    // bester gefundener Tauschkandidat: Zentrum
    int best_x;                                         // bester gefundener Tauschkandidat: Punkt
    int n;                                              // Index des nächsten Clusters
    int cur_center;                                     // akutell betrachtetes Zentrum 
    double min_td;                                      // Minimale TotalDeviation
    int argmin_td;                                      // Array Eintrag, der die TotalDevation minimiert
    bool member;                                        // zur Prüfung ob es sich bereits um ein Zetrum handelt

    for (int x_j = 0; x_j < conf->numb_objs; x_j++)
    {
        // Prüfe  ob x_j ist bereites Zentrum
        member = false;
        for (int check_k = 0; check_k < conf->k; check_k++)
        {
            if (conf->centers[check_k] == x_j)
            {
                member = true;
            }
        }
        // falls x_j bereits Zentrum ist kann es nicht reingetauscht werden
        if (member == false)
        {
            // Gewinn dadurch, dass x_j Zentrum wird
            // Delta TD = -d_j für alle Einträge;
            for (int i = 0; i < conf->k; i++)
            {
                td[i] = -weights[x_j] * get_distance(x_j, CENTROID(x_j), conf);
            }

            min_td = INFINITY; // beste Verbesserung initalisieren

            for (int x_o = 0; x_o < conf->numb_objs; x_o++) // iteriere über alle Punkte
            {
                // Prüfe ob x_o = x_j, wenn diese der Falls ist ist der Abstand 0
                if (x_o != x_j)
                {
                    d_oj = get_distance(x_o, x_j, conf); // Abstand zwischen x_o und x_j
                    n = conf->clusters[x_o]; // Index des nächsten Centers vom Punkt x_o
                    td[n] += weights[x_o] * (fmin(d_oj, get_distance(x_o, SECOND(x_o), conf)) - get_distance(x_o, CENTROID(x_o), conf)); // Änderung der Kosten

                    if (weights[x_o] * d_oj < weights[x_o] * get_distance(x_o, CENTROID(x_o), conf)) // wenn der Abstand zwischen x_o und x_j der bislang beste ist
                    {
                        for (int m_i = 0; m_i < conf->k; m_i++)
                        {
                            cur_center = conf->centers[m_i];
                            if (m_i != n) //m_i != m_n
                            {
                                td[m_i] = weights[x_o] * (d_oj - get_distance(x_o, CENTROID(x_o), conf)) + td[m_i]; // passe Kosten an
                            }
                        }
                    }
                }
            }

            //finde Besten gain und argmin
            for (int j = 0; j < conf->k; j++)
            {
                if (td[j] < min_td)
                {
                    argmin_td = j;
                    min_td = td[j];
                }
            }
            if (min_td < best_td)
            {
                best_td = min_td;
                best_x = x_j;
                best_medoid = argmin_td;
            }
        }
    }

    // fall die Lösung verbessert wird , führe den besten Tausch durch und passe Kosten an
    if (best_td < 0)
    {
        conf->centers[best_medoid] = best_x; // Bester Tauschkandidat (Zentrum) wird durch den Bersten Tauschkandidat (Punkt) ersetzt
        conf->cost += best_td; // Updaten der Kosten um die stärkste Verbesserung

        //  Update Zentrum und zweitnächste Punkte
        for (int i = 0; i < conf->numb_objs; i++)
        {
            // das Zentrum muss nur geupdatet werden, wenn das vorherige (zweit)nächste Zentrum entfernt wurde oder das neue Zentrum näher ist
            if (get_distance(conf->centers[best_medoid], i, conf) < get_distance(CENTROID(i), i, conf)) // neues Zentrum näher
            {
                // finde neues nächstes Zentrum
                determine_cluster(i, conf);
            }
            if (conf->clusters[i] == best_medoid) // nächstes Zentrum wurde entfernt
            {
                // finde neues nächstes Zentrum
                determine_cluster(i, conf);
            }
            else if (conf->second[i] == best_medoid) // zweitnächstes Zentrum wurde entfernt
            {
                // finde neues zweitnächstes Zentrum
                determine_cluster(i, conf);
            }
        }
    }
    free(td);
}
