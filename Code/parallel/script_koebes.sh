#! /bin/bash


export GUROBI_HOME=/opt/cs/gurobi911
export GRB_LICENSE_FILE=/opt/cs/share/gurobi.lic
export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:/opt/cs/gurobi911/lib

#################################

calc(){ awk "BEGIN { print "$*" }"; }

#################################
### Parameter festlegen
#################################
K=9
L=20
LL=6
folder=gray_many1000
if [ ! -d "./$folder" ]; ### prüfe ob Ordner existiert
then
	echo "Der Ordner existiert nicht"
	exit 1
fi
echo "Es sollen $L Unterordner erstellt werden"

#######################
###Kopieren des Ordners
#######################
copyfolder="$folder"_copy
mkdir $copyfolder
cp -a ./$folder/. ./$copyfolder/

#####################
# get number of files
#####################
n=$(ls -lR $folder/*.png | wc -l)
echo "Anzahl der Bilder im Ordner ist $n"

intervall=$((n / L))
echo "Elemente pro Ordner $intervall"

########################
### Erstellung der Ordner
########################
for (( i = 0; i < $L; i++ )) 
do 
	mkdir "$folder"_"$i"
	cd $copyfolder
	#echo $i
	#echo $((L - 1))
	if (( $i == $((L - 1)) ))
	then
		intervall=$(( n - (L-1) * intervall ))
	fi
	mv `ls *.png | head -"$intervall"` ../"$folder"_"$i"
	cd .. 
done

###############################
### Phase 1 des Algorithmus ###
###############################
for (( i = 0; i < L; i++ )) 
do 
	./phase1 $folder"_"$i centers$i $K &
done

#############################
### Warten auf alle Prozesse
#############################
wait
sleep 30s

echo "Alle Prozesse wurden beendet"

######################
### Weiterverarbeitung
######################
./merge $K $L centers

./prep_p2 $K $L $LL

for (( i = 0; i < $LL; i++ )) 
do 
	./phase2 $folder "zentren_"$i$".bin" $K $L $LL $i &
done
wait
sleep 30s

./merge2 $K $LL final_zentren
echo "Prozesse wurden zusammen geführt"

./phase3 $folder final_zentren.bin $K $L $LL

wait
rm -rf $copyfolder #Entfernung des Erstellten Ordners
rm *.bin
rm *.csv
for (( i = 0; i < $L; i++ )) 
do 
	rm -rf "$folder"_"$i"
done
