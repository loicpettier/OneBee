# OneBee

# Pour exécuter le code qui test le Lora et envoie une trame en boucle toute les 5 minutes :
cd /home/pi/Documents/Test LORA/codeblock/LoraProject
gcc -Wall -lwiringPi main main.c
sudo ./main

# Pour exécuter le code qui lance l’analyse de la porte et tourne en boucle :
cd /home/pi/Documents/onebee/Version 7
g++ -Wall -o camera camera.c -lrt ` pkg-config --cflags --libs opencv `
sudo ./camera
