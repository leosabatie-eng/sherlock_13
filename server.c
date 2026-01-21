#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>


#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>

struct _client
{
        char ipAddress[40];
        int port;
        char name[40];
} tcpClients[4]; //le jeu se joue à 4 joueurs
int nbClients; //compte les nb de clients connecter
int fsmServer;//machine d'état finis //////////////// si  on attend encore des joueurs si =1 on a le bon nb de joueurs
int deck[13]={0,1,2,3,4,5,6,7,8,9,10,11,12};//tableau des cartes
int tableCartes[4][8];//matrice pour le jeu  4=4joeurs 8 = 8colonnes calculer par le serveur principale
char *nomcartes[]=
{"Sebastian Moran", "irene Adler", "inspector Lestrade",
  "inspector Gregson", "inspector Baynes", "inspector Bradstreet",
  "inspector Hopkins", "Sherlock Holmes", "John Watson", "Mycroft Holmes",
  "Mrs. Hudson", "Mary Morstan", "James Moriarty"};
int joueurCourant;//quel joueurs doit jouer
int joueur_restant;//combien de joueurs ne sont pas éliminés
int eliminated[4];//liste des joueurs éliminés
int replayVotes[4];
int votesCount;
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void melangerDeck()
{
        int i;
        int index1,index2,tmp;

        for (i=0;i<1000;i++)//1000 échanges pour mélanger
        {
                index1=rand()%13;
                index2=rand()%13;

                tmp=deck[index1];
                deck[index1]=deck[index2];
                deck[index2]=tmp;
        }
}

void createTable()
{
	// Le joueur 0 possede les cartes d'indice 0,1,2
	// Le joueur 1 possede les cartes d'indice 3,4,5 
	// Le joueur 2 possede les cartes d'indice 6,7,8 
	// Le joueur 3 possede les cartes d'indice 9,10,11 
	// Le coupable est la carte d'indice 12
	int i,j,c;

	for (i=0;i<4;i++)
		for (j=0;j<8;j++)
			tableCartes[i][j]=0;

	for (i=0;i<4;i++)//matrice de jeu
	{
		for (j=0;j<3;j++)
		{
			c=deck[i*3+j];
			switch (c)
			{
				case 0: // Sebastian Moran
					tableCartes[i][7]++;
					tableCartes[i][2]++;
					break;
				case 1: // Irene Adler
					tableCartes[i][7]++;
					tableCartes[i][1]++;
					tableCartes[i][5]++;
					break;
				case 2: // Inspector Lestrade
					tableCartes[i][3]++;
					tableCartes[i][6]++;
					tableCartes[i][4]++;
					break;
				case 3: // Inspector Gregson 
					tableCartes[i][3]++;
					tableCartes[i][2]++;
					tableCartes[i][4]++;
					break;
				case 4: // Inspector Baynes 
					tableCartes[i][3]++;
					tableCartes[i][1]++;
					break;
				case 5: // Inspector Bradstreet 
					tableCartes[i][3]++;
					tableCartes[i][2]++;
					break;
				case 6: // Inspector Hopkins 
					tableCartes[i][3]++;
					tableCartes[i][0]++;
					tableCartes[i][6]++;
					break;
				case 7: // Sherlock Holmes 
					tableCartes[i][0]++;
					tableCartes[i][1]++;
					tableCartes[i][2]++;
					break;
				case 8: // John Watson 
					tableCartes[i][0]++;
					tableCartes[i][6]++;
					tableCartes[i][2]++;
					break;
				case 9: // Mycroft Holmes 
					tableCartes[i][0]++;
					tableCartes[i][1]++;
					tableCartes[i][4]++;
					break;
				case 10: // Mrs. Hudson 
					tableCartes[i][0]++;
					tableCartes[i][5]++;
					break;
				case 11: // Mary Morstan 
					tableCartes[i][4]++;
					tableCartes[i][5]++;
					break;
				case 12: // James Moriarty 
					tableCartes[i][7]++;
					tableCartes[i][1]++;
					break;
			}
		}
	}
} 

void resetGame() {
	printf("Resetting game for replay...\n");
	melangerDeck();
	createTable();
	joueurCourant = 0;
	joueur_restant = 4;
	for (int i=0; i<4; i++) {
		eliminated[i] = 0;
	}
}

void endGameAndProposeReplay(int winnerId, int guiltyCard) {
	char reply[256];
	if (winnerId != -1) {
		sprintf(reply, "W %d %d", winnerId, guiltyCard);
		broadcastMessage(reply);
		sleep(5); // Wait 5 seconds before sending the game over message
	
	}
	fsmServer = 2; // Go to post-game/voting state
	printf("Game over. Proposing replay.\n");
	broadcastMessage("P"); // Propose replay
	votesCount = 0;
	for (int j=0; j<4; j++) {
		replayVotes[j] = -1;
	}
}

void printDeck()
{
        int i,j;

        for (i=0;i<13;i++)
                printf("%d %s\n",deck[i],nomcartes[deck[i]]);

	for (i=0;i<4;i++)
	{
		for (j=0;j<8;j++)
			printf("%2.2d ",tableCartes[i][j]);
		puts("");
	}
}

void printClients()
{
        int i;

        for (i=0;i<nbClients;i++)
                printf("%d: %s %5.5d %s\n",i,tcpClients[i].ipAddress,
                        tcpClients[i].port,
                        tcpClients[i].name);
}

int findClientByName(char *name)//trouve où se trouve le joueur (si joueur 3 ---> joueur 2, joueur 1 =0)
{
        int i;

        for (i=0;i<nbClients;i++)
                if (strcmp(tcpClients[i].name,name)==0)
                        return i;
        return -1;
}

void sendMessageToClient(char *clientip,int clientport,char *mess)
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server = gethostbyname(clientip);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(clientport);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        {
                // This is not a fatal error. The client may have disconnected.
                // Do not exit the server.
                printf("Warning: could not connect to client %s:%d. It might be disconnected.\n", clientip, clientport);
                close(sockfd);
                return;
        }

        sprintf(buffer,"%s\n",mess);
        n = write(sockfd,buffer,strlen(buffer));

    close(sockfd);
}

void broadcastMessage(char *mess)//envoiyer le même message à tout le monde
{
        int i;

        for (i=0;i<4;i++)
		if (tcpClients[i].port != -1)
                sendMessageToClient(tcpClients[i].ipAddress,
                        tcpClients[i].port,
                        mess);
}

//retroune le prochain joueur
int nextPlayer(int current){
	int p = (current + 1) % 4;
	while(eliminated[p]){
		p = (p + 1) % 4;
	}
	return p;
}

int main(int argc, char *argv[])
{
	srand(time(NULL));//initialisation du générateur de nb aléatoire
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
	int i, j;
	joueur_restant = 4;

        char com;
        char clientIpAddress[256], clientName[256];
        int clientPort;
        int id;
        char reply[256];


     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);

	printDeck();
	melangerDeck();
	createTable();
	printDeck();
	joueurCourant=0;

	for (i=0;i<4;i++)
	{
        	strcpy(tcpClients[i].ipAddress,"localhost");
        	tcpClients[i].port=-1;
        	strcpy(tcpClients[i].name,"-");
		eliminated[i] = 0;
	}

     while (1)
     {    
     	newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen);
     	if (newsockfd < 0) 
          	error("ERROR on accept");

     	bzero(buffer,256);
     	n = read(newsockfd,buffer,255);
     	if (n < 0) 
		error("ERROR reading from socket");

        printf("Received packet from %s:%d\nData: [%s]\n\n",
                inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), buffer);

        if (fsmServer==0)
        {
        	switch (buffer[0])
        	{
                	case 'C':
                        	sscanf(buffer,"%c %s %d %s", &com, clientIpAddress, &clientPort, clientName);

				// Chercher un emplacement libre
				id = -1;
				for(i=0; i<4; i++) {
					if(tcpClients[i].port == -1) {
						id = i;
						break;
					}
				}

				if (id != -1) {
					printf("Player %s connected as player %d\n", clientName, id);
					strcpy(tcpClients[id].ipAddress,clientIpAddress);
					tcpClients[id].port=clientPort;
					strcpy(tcpClients[id].name,clientName);
					nbClients++;

					printClients();
				} else {
					printf("Server is full, connection from %s rejected.\n", clientName);
					break; // Ne rien faire si le serveur est plein
				}

				// rechercher l'id du joueur qui vient de se connecter

                                id=findClientByName(clientName);
                                printf("id=%d\n",id);

				// lui envoyer un message personnel pour lui communiquer son id

                                sprintf(reply,"I %d",id);
                                sendMessageToClient(tcpClients[id].ipAddress,
                                       tcpClients[id].port,
                                       reply);

				// Envoyer un message broadcast pour communiquer a tout le monde la liste des joueurs actuellement
				// connectes

                                sprintf(reply,"L %s %s %s %s", tcpClients[0].name, tcpClients[1].name, tcpClients[2].name, tcpClients[3].name);
                                broadcastMessage(reply);

				// Si le nombre de joueurs atteint 4, alors on peut lancer le jeu

                                if (nbClients==4)
				{
					printf("Tous les joueurs sont connectés, lancement du jeu !\n");
					for (int i=0; i < 4; i++)
					{
						sprintf(reply, "D %d %d %d", deck[0 + 3*i], deck[1 + 3*i], deck[2 + 3*i]);
						sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, reply);
							for (j = 0 ; j < 8 ; j ++) {
								sprintf(reply, "V %d %d %d", i, j, tableCartes[i][j]);
								sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, reply);
							}
					}
					sprintf(reply, "M %d", joueurCourant);//envoie qui est le joueur courant
					broadcastMessage(reply);

					// On envoie ses cartes au joueur 0, ainsi que la ligne qui lui correspond dans tableCartes
					// RAJOUTER DU CODE ICI

					// On envoie ses cartes au joueur 1, ainsi que la ligne qui lui correspond dans tableCartes
					// RAJOUTER DU CODE ICI

					// On envoie ses cartes au joueur 2, ainsi que la ligne qui lui correspond dans tableCartes
					// RAJOUTER DU CODE ICI

					// On envoie ses cartes au joueur 3, ainsi que la ligne qui lui correspond dans tableCartes
					// RAJOUTER DU CODE ICI

					// On envoie enfin un message a tout le monde pour definir qui est le joueur courant=0
					// RAJOUTER DU CODE ICI

                                        fsmServer=1;
				}
				break;
			case 'Q':
				sscanf(buffer, "Q %d", &id);
				if (id >= 0 && id < 4 && tcpClients[id].port != -1) {
					printf("Player %s (%d) disconnected from lobby.\n", tcpClients[id].name, id);
					strcpy(tcpClients[id].name, "-");
					tcpClients[id].port = -1;
					nbClients--;
					sprintf(reply,"L %s %s %s %s", tcpClients[0].name, tcpClients[1].name, tcpClients[2].name, tcpClients[3].name);
					broadcastMessage(reply);
				}
				break;
                }
	}
	else if (fsmServer==1)
	{
		switch (buffer[0])
		{
                	case 'G'://"guilty" options de jeu
				// RAJOUTER DU CODE ICI
						sscanf(buffer, "G %d %d", &id, &i);
						if (i == deck[12]){ //la 13 eme carte est le bon coupable
							endGameAndProposeReplay(id, i);
						}
						else{
							sprintf(reply, "E %d %d", id, i); //le joueur c'est trompé
							broadcastMessage(reply);
							eliminated[id] = 1;
							joueur_restant--;//un joueur de moins
							if (joueur_restant == 1){
								endGameAndProposeReplay(nextPlayer(joueurCourant), deck[12]);
							}
							else {//la partie continue
								joueurCourant = nextPlayer(joueurCourant);
								sprintf(reply, "M %d", joueurCourant);
								broadcastMessage(reply);
							}

						}
				break;
                	case 'O'://option "qui a cette carte"
				// RAJOUTER DU CODE ICI
						sscanf(buffer, "O %d %d", &id, &j);
						for (i = 0; i < 4; i++){
							if (i != id){//on demande a un autre joueur
								if (tableCartes[i][j] > 0)
								{
									sprintf(reply, "V %d %d 100", i, j); //on envoie 100 si le joueur a la carte
									broadcastMessage(reply);}
								else {
									sprintf(reply, "V %d %d 0", i, j); //on envoie 0 si le joueur n'a pas la carte
									broadcastMessage(reply);
							}	
						}
					}
						joueurCourant = nextPlayer(joueurCourant);
						sprintf(reply, "M %d", joueurCourant);
						broadcastMessage(reply);
				break;
			case 'S'://option de jeu: "toi joueur xxx combien as tu de cette carte"
				// RAJOUTER DU CODE ICI
				sscanf(buffer, "S %d %d %d", &id, &i, &j);
				sprintf(reply, "V %d %d %d", i, j, tableCartes[i][j]);
				broadcastMessage(reply);
				joueurCourant = nextPlayer(joueurCourant);
				sprintf(reply, "M %d", joueurCourant);
				broadcastMessage(reply);
				break;
			case 'Q':
				sscanf(buffer, "Q %d", &id);
				if (eliminated[id] == 0) {
					eliminated[id] = 1;
					joueur_restant--;

					// Broadcast player quit event without revealing cards
					sprintf(reply, "K %d", id);
					broadcastMessage(reply);

					if (joueur_restant == 1) {
						endGameAndProposeReplay(nextPlayer(joueurCourant), deck[12]);
					} else if (id == joueurCourant) {
						joueurCourant = nextPlayer(joueurCourant);
						sprintf(reply, "M %d", joueurCourant);
						broadcastMessage(reply);
					}

				}
				break;

            default:
                    break;
		}
        }
	else if (fsmServer == 2) // Voting state
	{
		switch (buffer[0])
		{
			case 'Y': // Vote Yes
				sscanf(buffer, "Y %d", &id);
				if (replayVotes[id] == -1) {
					printf("Player %d voted YES\n", id);
					replayVotes[id] = 1; // 1 for Yes
					votesCount++;
				}
				break;
			case 'N': // Vote No
				sscanf(buffer, "N %d", &id);
				if (replayVotes[id] == -1) {
					printf("Player %d voted NO\n", id);
					replayVotes[id] = 0; // 0 for No
					votesCount++;
				}
				break;
		}

		if (votesCount == nbClients && nbClients > 0) {
			printf("All players have voted.\n");
			bool all_yes = true;
			for (i=0; i<4; i++) {
				if (tcpClients[i].port != -1 && replayVotes[i] != 1) {
					all_yes = false;
					break;
				}
			}

			if (all_yes) {
				printf("All players voted YES. Restarting game.\n");
				resetGame();
				broadcastMessage("R"); // Tell clients to reset
				for (i=0; i < 4; i++) {
					sprintf(reply, "D %d %d %d", deck[0 + 3*i], deck[1 + 3*i], deck[2 + 3*i]);
					sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, reply);
					for (j = 0 ; j < 8 ; j ++) {
						sprintf(reply, "V %d %d %d", i, j, tableCartes[i][j]);
						sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, reply);
					}
				}
				sprintf(reply, "M %d", joueurCourant);
				broadcastMessage(reply);
				fsmServer = 1;
			} else {
				printf("Some players voted NO. Resetting to lobby.\n");

				// Send a specific command to each client and update server state.
				for (i=0; i<4; i++) {
					if (tcpClients[i].port != -1) {
						if (replayVotes[i] == 1) {
							// Tell YES voters to go back to the lobby
							sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, "B");
						} else {
							// Tell NO voters to close, then remove them from the server
							sendMessageToClient(tcpClients[i].ipAddress, tcpClients[i].port, "CLOSE");
							printf("Player %s (%d) is leaving.\n", tcpClients[i].name, i);
							strcpy(tcpClients[i].name, "-");
							tcpClients[i].port = -1;
							nbClients--;
						}
					}
				}

				fsmServer = 0; // Server will now wait for new connections or handling disconnections.
			}
		}
	}
     	close(newsockfd);
     }
     close(sockfd);
     return 0; 
}
