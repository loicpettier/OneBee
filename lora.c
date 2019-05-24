/*
 * gcc -Wall -lwiringPi -o lora lora.c
 *
 * */

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <sys/time.h>

int fd ;
unsigned int nextTime ;
int count ;
char buf2[100];

char c;
struct termios options ;

char data[]="mac tx uncnf 2 ";

char Date[20],Jour[20],Heure[20],Minute[20];

/*
Fonctions de bases de la communication
*/
int SetUART8 (void){
//Réglage et connection

    if ((fd = serialOpen ("/dev/ttyS0", 57600)) < 0)
    {
        printf ( "Unable to open serial device:\n") ;
        return 1 ;
    }

    tcgetattr (fd, &options) ;   // Read current options

    options.c_cflag &= ~CSIZE ;
    options.c_cflag &= ~CSTOPB ;
    options.c_cflag &= ~PARENB ;
    options.c_cflag |= CS8 ;
    options.c_cc[VMIN]=0;
    options.c_cc[VTIME]=1;

    tcsetattr (fd,TCSANOW ,&options) ;   // Set new options

	return 0;
}

void Receive(char buf2[100]){
    int iBcl=0;

    //printf("Reception:\n");
    memset(buf2,0,100);
    do{
      count=read(fd,&c,1);
      //printf("Count:%i\n",count);
      if(count==0){
        break;
      }
      else{
        buf2[iBcl]=c;
        iBcl++;
      }

    }while (count!=0);

    printf("\n\nRetour du module:\n%s\n\n",buf2);
    usleep(100000);
}

void Send(char* buf){
/////Transmitting Bytes

	count=write(fd,buf,strlen(buf)+1);
    printf("%s",buf);
	usleep(100000);

}

void Close(void){
	serialClose (fd);
}


void Trame(char * data2, int iE[10], int iS[10], int iTps){


    struct timeval curTime;
	gettimeofday(&curTime, NULL);
	char E[10],S[10],NbreMinute[10];
    char trame[200]={},tramehex[200]={};

    //recuperation de la date/heure avec le bon format
    //à cause de l'absence de module rtc, l'heure est perdue...
	strftime(Date, 80, "%y%m%d%H%M", localtime(&curTime.tv_sec)); // date au format aammjjhhmm

	//On stocks les infos dans la trame
	sprintf(NbreMinute,"%i",iTps);
	strcat(trame,NbreMinute);
    strcat(trame,Date);

    //Conversion d'entier en tableau de char
    for(int iBcl=0;iBcl<iTps;iBcl++){

        sprintf(S,"%i",iS[iBcl]);
        strcat(trame,S);

        sprintf(E,"%i",iE[iBcl]);
        strcat(trame,E);

    }

    //On convertie en hexa
    /*
    for(int iBcl=0;iBcl<strlen(trame);iBcl++) {
        char temp[5];
        sprintf(temp,"%x",trame[iBcl]);
        strcat(tramehex,temp);

    }*/

    //On concatene à la trame principale que l'on a dupliqué avant pour garder l'originale
    strcpy(data2,data);
	//strcat(data2,tramehex);
	strcat(data2,trame);//on garde la trme d'origine, sans conversion en hexa, elle ne fait que ralonger cette dernière
	strcat(data2,"\r\n");

}



int main(int argc, char **argv){

    //Etat des entrées sorties dans les 10 dernières minutes
    int iE[10]={0,1,2,3,4,5,6,7,8,9};
    int iS[10]={0,1,2,3,4,5,6,7,8,9};
    char buf2[200]={};
    int iCnt=0;
    //char buf[]="sys reset\r\n";
    //char buf[]="mac join abp\r\n";

    //Initialisation du module, on se connecte à la gateway

        if(SetUART8()==1){
            return 1;
        }

        printf("1 : join");
        Send("mac join abp\r\n");
        Receive(buf2);
        //Vérification pour être sûr d'être dans le réseau
        if(strcmp(buf2,"accepted\r\n")==0){
            return 1;
        }

        //sleep(20);



        Send("mac pause\r\n");
        Receive(buf2);
        Send("mac forceENABLE\r\n");
        Receive(buf2);
//        printf("2 : get sf");
//        Send("radio get sf\r\n");
//        Receive(buf2);
//
        printf("3 : set sf");
        Send("radio set sf sf7\r\n");
        Receive(buf2);

        printf("4 : get sf");
        Send("radio get sf\r\n");
        Receive(buf2);

        printf("5 : set f");
        Send("radio set freq 870000000\r\n");
        Receive(buf2);

        Send("mac resume\r\n");
        Receive(buf2);

         Send("mac save\r\n");
        Receive(buf2);

        sleep(30);

    while(1){
    //Début d'échange de trame
            iCnt++;
            printf("----------------------------\nEnvoi n:%i\n",iCnt);
            Trame(buf2, iE,iS,5);
            Send(buf2);//Envoi d'une trame
            Receive(buf2);//Les données lues sont stockées dans un buffer
            sleep(300);
        }

        /*Pour reinitialiser le module

        Send("sys reset\r\n");

        Receive(buf2);

        */
        //On ferme la communication
        Close();

	return 0;
}

