# für Koebes, mit dem entsprechenden Pfad zu dem Gurobi Ordner

#phase1
phase1:
	gcc -m64 -g histogram_dist.c main_phase1.c wasser_wh.c algorithm.c -o phase1 -I../../../../opt/cs/gurobi911/include/ -L../../../../opt/cs/gurobi911/lib -lgurobi91 -lm

phase2:
	gcc -m64 -g histogram_dist.c main_phase1.c wasser_wh.c algorithm.c -o phase1 -I../../../../opt/cs/gurobi911/include/ -L../../../../opt/cs/gurobi911/lib -lgurobi91 -lm

phase3:
	gcc -m64 -g histogram_dist.c main_phase1.c wasser_wh.c algorithm.c -o phase1 -I../../../../opt/cs/gurobi911/include/ -L../../../../opt/cs/gurobi911/lib -lgurobi91 -lm

merge:
	gcc merge.c -o merge -lm

merge2:
	gcc merge2.c -o merge2 -lm

prep_p2:
	gcc prep_p2.c -o prep_p2 -lm
