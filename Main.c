/* Code " compteur d'abeille "

     \     /
          \    o ^ o    /
            \ (     ) /
 ____________(%%%%%%%)____________
(     /   /  )%%%%%%%(  \   \     )
(___/___/__/           \__\___\___)
   (     /  /(%%%%%%%)\  \     )
    (__/___/ (%%%%%%%) \___\__)
            /(       )\
          /   (%%%%%)   \
               (%%%)
                 !
				 
Version : 1.1
Date : 24/05/2019
Createur : Jean-Camille Lebreton
Modification : Loïc Pettier
Description : Ce code a pour vocation de compter le mouvemeent des abeilles en temps réel
Pour cela on utilisera une détection par video grace a une caméra placée sur le module

LIGNE DE COMPILATION :

 g++ -Wall -o camera camera.c -lrt ` pkg-config --cflags --libs opencv `
 
 ATTENTION : 

A ajouter au lancement de la raspberry : 
sudo modprobe bcm2835-v4l2 (Pour la PI2) 

*/

//g++ -Wall -pthread -o onebee light.c -lpigpio -lrt  `pkg-config --cflags --libs opencv` `mysql_config --cflags --libs` 

#include "opencv2/core/core.hpp" //bibliotheque générale d'opencv
#include "opencv2/highgui/highgui.hpp" //bibilotheque auxilliaire(traitement d'image)
#include "opencv2/imgproc/imgproc.hpp" //bibliotheque auxilliaire(affochage des images)
#include "opencv2/opencv.hpp" // Root des bibilotheques
#include <stdlib.h>
#include <stdio.h>
#include <iostream> //bibliotheque de gestion des entrées video
#include <string.h>
#include <sys/time.h> //bibliotheque interne a la raspberry (permet de recupere la date et l'heure de la raspberry
#include <sys/io.h>
//#include <pthread.h> //bibliotheque de gestion des thread processeurs (au nombre de 4)
#include <sys/fcntl.h>
#include <sys/types.h>
#include <unistd.h>
//#include <mysql/mysql.h>
//#include <pigpio.h> //bibliotheque pour la gestion des GPIO de la rpi 

using namespace std;
using namespace cv;

//Variables de test pour compter le temps entre deux boucles//
//////////////////////////////////////////////////////////////

void calib_auto();//foncion de calibration automatique
void get_time(); //fonction de récupération de l'heure et de la date (gere aussi l'initialisation des variables correspondantes (Heure,Minute,Day ... etc)
void passage(int i, int *entree, int *sortie); //fonction permettant de compter le passage des abeilles (le i correspond a l'indice de l'image que nous passons)
void suppressbruit(Mat Pic); //fonction générique qui permet de supprimer le bruit dans une image
void recup_fond(void); // Fonction permettant de creer les fonds . Ceci évitera de prendre en compte des glitchs ou des depots sur les passages.
void recup_fond_continu(void); // Fonction permettant de mettre a jour les fonds . Ceci évitera de prendre en compte des glitchs ou des depots sur les passages.

void sauvegarde(); //fonction de sauvegarde (tout les x minutes (choisi apr l'utilisateur)) uniquement les ficher .csv


//--------DECLARATION DES VARIABLES -------------------------------------------------------

//initialisation par defaut(obsolete mais présent au cas ou un sous nombre de porte serait utilisé
int Y[40]={0};
int X[3]={0};

int Quitter=0; //varibales flag permettant de quitter le programme sur demande (debug only)
int C=0; //variable flag permettant d'entrer dans la calibration des portes (debugo only)
int video=0;// variable permettant de determiner l'affichage que nous voulons( 0=rien, 1=video, 2=image fixe, 3=graphique)
int quitterC=0;//variable flag permettant de quitter la calibration automatique
int T_variable[100]={0}; //(dans fonction recall) permet de recupere dans un tableau les valeurs de fichier "Sauvegarde.txt" qui est le fichier de sauvegarde de la calibration
int T_raw[24000]={0}; //(dans fonction sauvegare_graphique) tableau tres lourd, permettant de mettre en memoire la totalitée de la sauvegarde journaliere pour créer le graphique au format une case pour un chiffre
int T_data[7500]={0}; //(dans sauvegarde_graphique) tableau plus leger contenant les données des mesures de la journée au format une case pour un valeur (
int nombreporte=0; //variable qui porte le nombre de porte cette variable est initialisée dans calib_auto

//---------Creation de toutes les images--------------------

Mat hsvcrop[40]; //tableau contenant toutes les petites images apres leur traitement HSV
Mat sourcecrop;  
Mat source,hsvsource,masquesource,src,masquesourcebleu,masquesourcerouge,masquesourcerouge1,masquesourcerouge2; //declaration des matrices d'images principales (pour le debug)
Mat masquecropTotal[40]; //tableau contenant toutes les petites images apres le traitement du filtre de couleur
Mat sourcegray; //HSV 42  153 115
Mat canny_out[30];
Mat detection[30];
Mat source_crop[30];

Mat masquecropJaune[20]; //HSV 5   153 115
Mat masquecropVert[20];	 //HSV 81  153 115
Mat masquecropBleu[20];  //HSV 170 153 115
Mat masquecropViolet[20];//HSV 210 153 115
Mat cropedImage[20]; //tableau contenant toutes les originaux des petites images
Mat im_menu; //declaration d'une matrice pour l'image principale
Mat foreground[30];
Mat fond[30];

Mat Comptage[20];

VideoCapture capture(0); //initialisation du flux(on le met ici car toutes les fonctions profiterons du flux video sans redéclaration

//////////////////////////////////////////////////////////////

//------------Creation des variables de traitement d'image----------------


Mat Lignes = Mat::zeros( source.size(), CV_8UC3 ); //permet le dessin de lignes (pr le debug)

//////////////////////////////////////////////////////////////

// ---------- Declaration des fonds pour la detection des abeilles --------------------------------------------
int history = 1000;
int varThreshold = 16.f;
BackgroundSubtractorMOG2 bg[24] = BackgroundSubtractorMOG2(history,varThreshold,false);

//////////////////////////////////////////////////////////////


int ZoneEntree[16][4]={{0}}; // Déclaration d'un tableau pour les zones d'entrée regroupant le numero de la zone comme index et ses 4 parametres comme coordonées
int ZoneSortie[16][4]={{0}};// Déclaration d'un tableau pour les zones de sorties regroupant le numero de la zone comme index et ses 4 parametres comme coordonées
char delimiteur[16][2]={{0}};
char flagSens[16]={0};
char flagAccept[16]={0};

// ---------- declaration de toutes les variables de chaque image crées nous permet de creer les vecteurs de déplacement des abeilles --------

int CumulEntree=0,CumulSortie=0;//variables comptant les entrée sorties des abeilles sur une journée complète.

int LastXTotal[40]={-1}, LastYTotal[40]={-1}, posXTotal[40],posYTotal[40]; //tableau de variables (1case /image) permettant de tracer le mouvement d'une abeille dans un canal en temps réel sur un ecran <=> position actuelle de l'abeille
int LastXBleu[40]={-1}, LastYBleu[40]={-1}, posXBleu[40],posYBleu[40];
int LastXVert[40]={-1}, LastYVert[40]={-1}, posXVert[40],posYVert[40];
int LastXJaune[40]={-1}, LastYJaune[40]={-1}, posXJaune[40],posYJaune[40];
int LastXViolet[40]={-1}, LastYViolet[40]={-1}, posXViolet[40],posYViolet[40];
int LastXRouge[40]={-1}, LastYRouge[40]={-1}, posXRouge[40],posYRouge[40];

int flagTotal[40]={0}; //flag de passage evite de compter 1000000 sorties alors que c'etait juste une abeille qui attendais dans la mauvaise zone .... sa****
int flagBleu[40]={0}; 
int flagVert[40]={0}; 
int flagJaune[40]={0}; 
int flagViolet[40]={0}; 
int flagRouge[40]={0}; 

//////////////////////////////////////////////////////////////

int bisYTotal[40]={0}; //sauvegarde différée de la position de l'abeille. Pour detecter correctement le mouvement position a T+1
int bisYBleu[40]={0};
int bisYVert[40]={0};
int bisYJaune[40]={0};
int bisYViolet[40]={0};
int bisYRouge[40]={0};

int deplacementTotal[40]={0}; //variable etant plus simplement le sens du vecteur (son signe etant la seule chose qui nous importe)
int deplacementBleu[40]={0};
int deplacementVert[40]={0};
int deplacementJaune[40]={0};
int deplacementViolet[40]={0};
int deplacementRouge[40]={0};


//-------------------------variables communes au programme-----------------------

int initialisation=0; // variable permettant de detecter si l'initialisation a déja été effectuée avant
int entreeTotal=0,sortieTotal=0; //variables comptant les entrée sorties des abeilles
int entreeRouge=0,sortieRouge=0;
int entreeVert=0,sortieVert=0;
int entreeBleu=0,sortieBleu=0;
int entreeJaune=0,sortieJaune=0;
int entreeViolet=0,sortieViolet=0;
int totalentree=0,totalsortie=0; //variables comptant les entrée sorties des abeilles sur une journée complète.

int Entree=0,Sortie=0;

char sentreeTotal[4]={0},ssortieTotal[4]={0};
char sentreeRouge[4]={0},ssortieRouge[4]={0};
char sentreeBleu[4]={0}, ssortieBleu[4] ={0};
char sentreeVert[4]={0}, ssortieVert[4] ={0};
char sentreeJaune[4]={0},ssortieJaune[4]={0};
char name[30]; //variable permettant de créer les affichages des entrées sorties
int flagdetectcouleur=1; // Variable permettant de choisir si l'on souhaite utiliser la multiple détection de couleur ou non
//////////////////////////////////////////////////////////////

// --------- variables pour recupere l'heure permettant la sauvegarde -----------

static int seconds_last = 99; //variable permettant de determiner le changement de seconde(chargé avec 99 aléatoirement pour entrer une premeire fois dans la boucle)
char Heure[20],DateString[20],Jour[20],Minute[20],HeureMinute[20],Time[20],sDate[30]; //variables dont nous allons nous servir dans tout le programme et nous permettant de mettre l'heure et la date dans des variables lisibles
string oldday="\0",oldminute="\0",oldhour="\0"; //variables de flag permettant de determiner si nous changeons de jour ou non.

//////////////////////////////////////////////////////////////
///// Variables utiles a la sauvegarde .//////////


FILE *file; //fichier de sortie des detections
char nom[100]; //tableau sauvegardant le nom du ficher de facon dynamique(le nom est changant a hauteur d'une fois par jour)[sauvegarde serveur]

///////////////////////////////////////////////////////////////

// Il faut rajouter une RTC a la PI !!


void get_time()//fonction nous permettant de recuperer la date et l heure de la raspberry
{
/*
	Présentation: Ceci est une fonction générique et modifiée permettant d'acceder a la date et l'heure de la raspberry
	Explications:
	1- nous récupérons la date actuelle
	2- on test voir si nous sommes a une nouvelle date (ici 1seconde plus tard)
	3- on met a jour notre flag de detection de nouvelle data
	4- nous récuperons et formatons toutes les odnénes de dates comme nous en avons besoin
	Précisions : Cette fonction est GENERIQUE elle marche sur tout les raspberry par defaut aucun paquet n est nécessaire
*/
	timeval curTime;
	gettimeofday(&curTime, NULL);
	if (seconds_last == curTime.tv_sec)
	return;
	
	seconds_last = curTime.tv_sec;
	
	strftime(DateString, 80, "%d-%m-%Y", localtime(&curTime.tv_sec));
	strftime(HeureMinute, 80, "%H:%M", localtime(&curTime.tv_sec));
	strftime(Jour, 80, "%d", localtime(&curTime.tv_sec));
	strftime(Minute, 80, "%M", localtime(&curTime.tv_sec));
	strftime(Heure, 80, "%H", localtime(&curTime.tv_sec));
	printf("Minute : %s \n",Minute);
}

void sauvegarde() //contient aussi la sauvegarde de secours
{
/*
	Présentation: Voici la fonction de sauvegarde automatique des données. Cette sauvegarde s'occupe uniquement des
	données .csv pour une utilisation dans exel ou tout autre logiciel similaire.
	Explications:
	1-Si nous sommes un nouveau jour, nous recréeons un fichier vierge qui contiendra les données de la journée
	2-Ensuite nous fesons un test pour voir si nous sommes a une nouvelle minute de l'heure
	3-On teste notre compteur de minute pour voir s'il n est pas different de zero (si c est le cas aucune sauvgarde
	n est faite
	4-On teste ensuite notre compteur pour voir si nous sauvegarde a l'interval demandé par l'utilisateur
	5-Nous sauvegardons dans le fichier sous un format Heure:Minute entree sortie
	6-On reset les compteurs pour les sauvegardes ulterieures
	Précisions: Dans l absolu nous sauvegardons a deux endroits : dans le dossier du programme et dans la dossier pour
	la communication avec l exterieur
*/
	if(oldday!=Jour)
	{
		snprintf(nom,sizeof(nom),"/home/pi/Documents/Compteur/%s.csv",DateString);///var/www/html/
		file=fopen(nom,"a+");	
		oldday=Jour;	
		fclose(file);
	}
	if(oldminute!=Minute)
	{
		file=fopen(nom,"a+");	
		oldminute=Minute;
	
		fprintf(file ,"%s;%d;%d;\n",HeureMinute,Entree,Sortie);
		fclose(file);
		Entree = 0;
		Sortie = 0;
		printf("Sauvegarde effectuée \n");
	}
	if(oldhour!=Heure)
	{
		//protocole d'envoi I2C
		//protocole pour bypasser les soucis du lora --> au besoin cette fonction sera remplacée par des commandes uart
	}

}

void calib_auto() //Validé ! 
{
/*
	Présentation :
	Cette fonction ne prenant aucun de parametres et ne retournant rien nous permet d effectuer une calibration automatique
	des portes d'entrées sortie. 
	Explication : 
	1-Nous prennons l'image sortant du flux video(qui est normalement l'entrée de la ruche vue du dessus)
	2-Nous traitons cette image pour ne garder que la couleur "rouge"
	3-Nous prenons une ligne de l'image et nous sucrutons la totalitée de cette ligne
	4-Nous scrutons les données au fur et a mesure qu'elle arrivent et les rangeons dans un tableau.
	Précisions:
	etape 4: -> en toute logique, nous avons dans le tableau tout les moments où la ligne de pixel change de couleur
	cad que lorsque que l'on a decouvert le pixel 0 nosu enregistrons l'endroit ou nous sommes dans la ligne
	et ensuite nous ne fesons rien mais des que nous trouvons un pixel a 255 nous reenregistrons cette position qui
	marquera la fin de la detection de la porte "1".
*/


	int flag0=0,flag255=0,ecart=0,matj=0,nbporte=0,i=0,milieu=0;
	int calibauto[80]={0};
	int zone=0;


do{
	sleep(2);
	capture >> source;	
	waitKey(1);
	capture >> source;	
	
	
	//imshow("video",source);
	cvtColor(source,hsvsource,CV_BGR2HSV);//conversion de BRG en HSV
	inRange(hsvsource,Scalar(80,0,0,0),Scalar(150,255,255,0),masquesource);//bleu
	suppressbruit(masquesource);
	//imshow("masksourceB",masquesource);//ligne a decommenter si besoin est de verifier la bonne detection du bleu.
	sleep(10);
	
	
	printf("resolution : %d x %d \n",masquesource.cols,masquesource.rows);

		for(matj=0;matj<masquesource.rows;matj++)
		{	
			//printf("matj = %i\n",matj);
			//printf("masquesource.at<uchar>(matj,320) = %i\n",masquesource.at<uchar>(matj,(masquesource.cols/2)));
			
			//printf("flag0 = %i\n",flag0);
			//printf("ecart = %i\n",ecart);
			switch(masquesource.at<uchar>(matj,(masquesource.cols/2)))
			{
				case 0:
				if(flag0==0 && ecart >10)
				{
					//printf("pixel : %d || position dans le tableau : %d \n",matj,nbporte);
					flag0=1;flag255=0;ecart=0;
					calibauto[nbporte]=matj;nbporte++;
				}break;
				
				case 255:
				if(flag255==0 && ecart >10)
				{
					//printf("pixel : %d || position dans le tableau : %d \n",matj,nbporte);
					flag0=0;flag255=1;ecart=0;
					calibauto[nbporte]=matj;nbporte++;
				}break;
				
			}
		ecart++;
			
		}
	X[0]=calibauto[2]-2;
	X[1]=calibauto[3]-2;
	X[2]=(X[0]+X[1])/2;
	
	// ici on remet les flags dans leurs conditions initiales 
	ecart=0;flag0=0;flag255=0;nbporte=0;
	// tmp ( a renommer) est le milieu de la porte bleue : il permet de faire le milieu dans l'axe horizontal.
	milieu=X[2];
	
	//printf("milieu : pixel num %d \n",milieu);
	
	//DEBUG si besoin est, ce debug permet un affichage dynamique du milieu de la porte bleue. 
	/*
		while(1)
	{
		capture >> source;	
		line(source,Point(0,X[0]),Point(640,X[0]),Scalar(0,255,0),1); //top line	
		line(source,Point(0,X[1]),Point(640,X[1]),Scalar(0,255,0),1); //Bottom line
		line(source,Point(0,X[2]),Point(640,X[2]),Scalar(0,255,0),1); //middle line
		imshow("video",source);
		waitKey(1);
	}
	*/
	//FIN DEBUG
	
	cvtColor(source,hsvsource,CV_BGR2HSV);
	
	/*
	inRange(hsvsource,Scalar(0,60,0,0),Scalar(30,255,255,0),masquesourcerouge1);
	inRange(hsvsource,Scalar(130,60,0,0),Scalar(180,255,255,0),masquesourcerouge2);
	add(masquesourcerouge1,masquesourcerouge2,masquesource);	
	*/
	
	inRange(hsvsource,Scalar(70,20,0,0),Scalar(180,255,255,0),masquesource);
	suppressbruit(masquesource);
	//imshow("masksourceR",masquesource);
	
	
	
		for(matj=0;matj<masquesource.cols;matj++)
		{	
			switch(masquesource.at<uchar>(milieu,matj))
			{
				case 0:
				if(flag0==0 && ecart >7)
				{
					//printf("%d %d \n",matj,nbporte);
					flag0=1;flag255=0;ecart=0;
					calibauto[nbporte]=matj;nbporte++;
				}break;
				
				case 255:
				if(flag255==0 && ecart >7)
				{
					//printf("%d %d \n",matj,nbporte);
					flag0=0;flag255=1;ecart=0;
					calibauto[nbporte]=matj;nbporte++;
				}break;
				
			}
		ecart++;
			
		}
		// A la fin de cette boucle nous avons toutes les portes de détectées. Nosu remarquerons toutefois que deux portes supplémentaires ce sont rajoutées
	// au debut et a la fin des passages. ce sont des portes factices et n'ont pas lieu d'être utilisées. il faut donc les supprimer.
	
	//on commence par supprimer les portes non voulues : la première portes <=> Les 2 premières itérations.
	for(i=0;i<nbporte;i++)
	{
		calibauto[i]=calibauto[i+2];

	}
	////////////
	nombreporte=(nbporte-3)/2;
	printf("nombre de passages %d \n",nombreporte);
	//on range dans un autre tableau les valeurs (passage en global)
	for(i=0;i<nombreporte*2;i++)
	{
		Y[i]=calibauto[i];
		printf("pos %d:%d \n",i,Y[i]);
	}
	
	
	//DEBUG - permet de visualiser les differentes colonnes
	/*	
	 while(1)
	{
	 capture >> source;
	 for(i=0;i<nbporte;i++)
		{
			line(source,Point(Y[i],0),Point(Y[i],480),Scalar(0,255,0),1);
		}
		line(source,Point(0,X[0]),Point(640,X[0]),Scalar(0,255,0),1); //top line	
		line(source,Point(0,X[1]),Point(640,X[1]),Scalar(0,255,0),1); //Bottom line
		line(source,Point(0,X[2]),Point(640,X[2]),Scalar(0,255,0),1); //middle line
		imshow("video",source);

		waitKey(1);
	}
	*/
	//FIN DEBUG 
	
	for(zone=0;zone<16;zone++)
	{
		//printf("zone %d \n",zone);
		ZoneEntree[zone][0]=Y[zone*2];   	// correspond a la limite gauche
		ZoneEntree[zone][1]=Y[zone*2+1]; 	// correspond a la limite droite
		ZoneEntree[zone][2]=X[2];		 	// correspond a la limite haute  
		ZoneEntree[zone][3]=X[1];		 	// correspond a la limite basse 
	}
	
	for(zone=0;zone<16;zone++)
	{
		//printf("zone %d \n",zone);
		ZoneSortie[zone][0]=Y[zone*2];		// correspond a la limite gauche
		ZoneSortie[zone][1]=Y[zone*2+1];	// correspond a la limite droite
		ZoneSortie[zone][2]=X[0];			// correspond a la limite haute
		ZoneSortie[zone][3]=X[2];			// correspond a la limite basse 	
	}
	
	//DEBUG	
	
	for(zone=0;zone<16;zone++)
	 {
		 rectangle(source,Point(ZoneEntree[zone][0],ZoneEntree[zone][2]),Point(ZoneEntree[zone][1],ZoneEntree[zone][3]),Scalar(255,255,0,50),-1);
		 rectangle(source,Point(ZoneSortie[zone][0]+1,ZoneSortie[zone][2]),Point(ZoneSortie[zone][1],ZoneSortie[zone][3]),Scalar(255,0,0,30),-1);
	 }
	imshow("Zones",source);
	//while(1)
	//{
		waitKey(1);
	//}
	
	
}while(nombreporte!=16);	
}




void passage(int i, int *entree, int *sortie)
{
/*
	Présentation : Cette fonction nous permet de compter le nombre de passage d'une abeille dans une porte. Les variables
	etant communes au programme la valeur"entree' et "sortie" compte les entrées sorties de toutes les abeilles de toutes
	les portes
	Explications:
	1- Nous determinloopons le sens detime_sleep déplacement des abeilles 
	2- Nous regardons dans quelles zones elles sont et distingons 3 cas (dans la zone "entrée", dans la zone"sortie" et 
	dans la zone "rien"
	3- Esuite si le mouvement respecte la postition, nous determinons le cas dans lequel nous sommes.
	4- Enfin on detecte que l'abeille quitte bien la zone de détection pour eviter un comptage inutile
	5- Pour finir, nous enregistrons la derniere position de l'abeille pour determiner ensuite son nouveau mouvement
	Précisions: 
	C'est comme se servir d'un vecteur ou nous cherchons a detecter sa direction, son amplitude n'ayant aucun effet. 
*/
if(delimiteur[i][0]==0 && delimiteur[i][1]==1 && flagSens[i] != 1)
	{
		flagSens[i] = 2;
		printf("Entree \n");
	}
	
	if(delimiteur[i][0]==1 && delimiteur[i][1]==0 && flagSens[i] != 2)
	{
		flagSens[i] = 1;
		printf("Sortie \n");
	}
	
	// on regarde si l'abeille un bien passé la limite
	if(delimiteur[i][0]==1 && delimiteur[i][1]==1)
	{
		flagAccept[i] = 1;
		printf("n%d accepted\n",i); 
	}
	
	//si elle a passé la limite et qu'elle n'est plus présente dans le couloir de détection, cette dernière est comptabilisée
	if(delimiteur[i][0]==0 && delimiteur[i][1]==0 && flagAccept[i]==1)
	{
		if(flagSens[i]==2)
		{
			*entree=*entree+1;
			flagSens[i]=0;
			flagAccept[i]=0;
		}
		 else if(flagSens[i]==1)
		{
			*sortie=*sortie+1;
			flagSens[i]=0;
			flagAccept[i]=0;
		}
	}
	
	return;
}

void ColorZone(int i,Mat image, int zone)//dessine les lignes pour suivi d objet
{
/*
	Présentation: on pourrait croire cette fonction inutile vu son nom.. mais en fait elle est le coeur de la detection
	En effet elle permet de determiner le centre de l'abeille lors de son passage dans la porte.
	Explications:
	1-On défini un moment et nous nous en servons pour determiner une position relative du point dans l'image
	2-On se sert de ces moments pour recuperer la coordonnée du point que nous enregistrons dans une variable
	3-On fait un test improbable de sécuritée pour eviter d'avoir des données n'existant pas(negatives)
	4-On met en tampon la position de l'abeille.
	Précisions: l'affichage de la ligne rouge n'est PAS obligatoire. elle est la pour présentation de la detection et
	ne consomme aucune ressource processeur (ou tellement infime qu'elle est négligeable...
*/
	Moments Moments = moments(image);

  	double M01 = Moments.m01;
 	//double M10 = Moments.m10;
 	double Area = Moments.m00;
	
	//int posX;
	int posY;
	
    // si area <= 400, cela signifie que l'objet est trop petit pour etre detecté 
	if (Area > 200)
 	{
	//calcule le centre du point
	if(zone == 1)
	{
		//posX = (M10 / Area)+X[0]; 
		posY = (M01 / Area)+Y[i*2]; 
	}
	else
	{
		//posX = (M10 / Area)+X[2]; 
		posY = (M01 / Area)+Y[i*2]; 
	}

	//printf("pos X : %d \n pos Y : %d \n",posX,posY);	
	

	
        if (posY>ZoneEntree[i][0] && posY<ZoneEntree[i][1])
   		{
    		// dessine un rectangle dans la zone où l'abeille est detectée 
			
				if (zone == 1)
				{
					delimiteur[i][0]=1;
					rectangle(source, Point(ZoneEntree[i][0],ZoneEntree[i][2]),Point(ZoneEntree[i][1],ZoneEntree[i][3]), Scalar(255,0,255), -1); 
				}
				if(zone == 2)
				{
					delimiteur[i][1]=1;
					rectangle(source, Point(ZoneSortie[i][0],ZoneSortie[i][2]),Point(ZoneSortie[i][1],ZoneSortie[i][3]), Scalar(0,0,255), -1); 
				}
   		}
		
		
  	}
	else
	{
		// si on ne detecte rien ou que c est trop petit on remet les variables a zero pour prouver au programme la non presence d abeilles	
		delimiteur[i][0]=0;
		delimiteur[i][1]=0;
	}
  	//imshow("flux_video", source); //show the original image
	
}

void suppressbruit(Mat Pic)
{
/*
	Présentation : Ceci est une fonction donnée par OpenCV. Elle permet de réduire le bruit des images que nous
	traitons. En quelques sortes en regarde dans ce qui entoure un pixel et l'on regarde quelle couleur est la plus 
	présente pour changer la couleur de ce dernier.
	Explications : N/A
	Précisions : N/A

*/
	blur(Pic,Pic,Size(3,3));

}

void recup_fond(void) //OK
{
	int aqui=0;
	int i=0;
	printf("Aquisition du fond .... \n");
	while ( aqui <= 100)
	{
		capture >> source;
		waitKey(1);

		for(i=0;i<nombreporte;i++)
		{
			//printf("nombre de passages %d \n",i);
			//on récupère le fond de la zone qui nous interesse (partie gauche))
			//printf("%d\n",i);
			//printf("val1 :%d -- val2 :%d->%d --val3 :%d -- val4 :%d->%d \n",X[0],Y[i*2],i*2,X[1]-X[0],Y[i*2+1]-Y[i*2],i*2+1);
			bg[i](source(Rect(Y[i*2],X[0],Y[i*2+1]-Y[i*2],X[1]-X[0])), foreground[i]); 	
			
		}
		
		aqui++;
		//30secondes environ
		if(aqui%100 == 0)
		{
			printf("...\n");
		}
		
	}
	for(i=0;i<nombreporte;i++)
	{
		//printf("nombre de passages %d \n",i);
		bg[i].getBackgroundImage(fond[i]); // Récupération du fond obtenu par l'aquisition précédente
		cvtColor(fond[i],fond[i],CV_BGR2GRAY);  // on passe en nuance de gris (facilité de détection)		
		
	}

	printf("Fin aquisition!\n");
}

void recup_fond_continu(void)
{
	int i=0;
	for(i=0;i<nombreporte;i++)
	{
		if(delimiteur[i][0]==0 && delimiteur[i][1]==0)
		{
			bg[i](source(Rect(Y[i*2],X[0],Y[i*2+1]-Y[i*2],X[1]-X[0])), foreground[i]);
			bg[i].getBackgroundImage(fond[i]); // Récupération du fond obtenu par l'aquisition précédente
			cvtColor(fond[i],fond[i],CV_BGR2GRAY);  // on passe en nuance de gris (facilité de détection)
		}
	}
}


int main(int argc, char **argv)
{	


	//     Varaibles internes au Main    //
	int i=0;
	char Affichage[50]={0};	
	//int thread_id=0;
	

	//     Initialisation du flux video  //
	if(!capture.isOpened()){
	printf("impossible d'initialiser le flux video\n verifiez les branchements");
	return -1;
	}

	//     Initialisation et déclaration des threads
	//pthread_t thread[4];
	

	capture >> source;//Une premiere capture d'image pour notre fonction de calibration
	calib_auto();//nous recuperons le nombre de porte ici et leur positionnement
	
	recup_fond(); //recuperation du fond
	
 	while(capture.read(source))
	{
		//printf("Démarrage fini\n");
		//on garde seulement la partie qui nous interesse et qui est de la meme dimension que la matrice de référence
		sourcecrop = source(Rect(Y[0],X[0],Y[31]-Y[0],X[1]-X[0]));		
		//on colore la zone récupérée en gris (pour effectuer la différence entre le fond et le flux d'image
		//printf("Recoupage fini\n");
		cvtColor(sourcecrop,sourcegray,CV_BGR2GRAY); //convertion en gris
		//printf("Conversion en gris fini\n");
		imshow("sourcegray",sourcegray);
		
		///---------------
		
		// Ici nous récupérons les 11 zones de détection et effectuons en plus la treshold pour obtenir la tache blanc sur noir que représente l'abeille.
		//------------------
		//printf("nombre de passages %d \n",nombreporte);
		for(i=0;i<nombreporte;i++)
		{
			//printf("Recup des zones %i\n",i);
			source_crop[i] = sourcegray(Rect(Y[i*2]-Y[0],0,Y[i*2+1]-Y[i*2],X[1]-X[0]));//Découpe l'image en portions ( "nombreporte" au total)
			absdiff(fond[i],source_crop[i],detection[i]); // detection des différences entre l'image de référence et l'image recue
			threshold(detection[i],canny_out[i],20,255,THRESH_BINARY);//effectue le threshold entre les deux images (flux et fond) retourne une imaga noir et blanc où l'abeille est récupérée comme un point blanc.
			suppressbruit(canny_out[i]); //suppression du bruit
		}
		//printf("fin premier for\n");	
		 imshow("1",canny_out[0]);
		 imshow("2",canny_out[1]);
		 imshow("3",canny_out[2]);
		 imshow("4",canny_out[3]);
		
		
		//------------------
		// Boucle effectuant le comptage
		//-------------------
		for(i=0;i<nombreporte;i++)
		{
			Comptage[i] = canny_out[i](Rect(0,0,Y[i*2+1]-Y[i*2],X[2]-X[0])); //on ne prend que la partie de gauche
			ColorZone(i,Comptage[i],1);// determine si il y a présence ou non d'une abeille du coté gauche (coté ruche)			
			Comptage[i] = canny_out[i](Rect(0,X[2]-X[0],Y[i*2+1]-Y[i*2],X[2]-X[0])); //on ne prend que la partie de droite
			ColorZone(i,Comptage[i],2);// détermine si il y a presence ou non d'une abeille du côté droit (côté extérieur)
			passage(i,&Entree,&Sortie);	// Partie comptage. explications dans la fonction	
		}		
		//printf("fin deuxième for\n");
		// Cette fonction permet de récupérer le fond de chaque passage en fonction de l'état de la porte.
		recup_fond_continu();
		//printf("fin recup fond\n");
		// Affichage des données sur le flux vidéo
		sprintf(Affichage,"Entrees : %d ",Entree);
		putText(source,Affichage,Point(20,20),FONT_HERSHEY_SIMPLEX,0.7,Scalar(255,0,0));
		sprintf(Affichage,"Sorties : %d",Sortie);
		putText(source,Affichage,Point(20,50),FONT_HERSHEY_SIMPLEX,0.7,Scalar(255,0,0));
		imshow("stream",source);
		waitKey(1);//dure 8ms normalement que 1ms <- normal, cette fonction attends au moins 1ms
		//printf("fin ecriture dans le fichier\n");
		//get_time();
		//sauvegarde();
		
	} 
	
	printf("bonsoir\n");
	return 0;
	
}
