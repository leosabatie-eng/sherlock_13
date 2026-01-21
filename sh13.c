#include <SDL.h>        
#include <SDL_image.h>        
#include <SDL_ttf.h>        
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>

pthread_t thread_serveur_tcp_id;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char gbuffer[256];
char gServerIpAddress[256];
int gServerPort;
char gClientIpAddress[256];
int gClientPort;
char gName[256];
char gNames[4][256];
int gId;//id du joeur
int joueurSel = -1;
int objetSel;
int guiltSel;
int guiltGuess[13];
int tableCartes[4][8];
int b[3];
int goEnabled;
int connectEnabled;
int coupable = -1; //carte du coupable
int gagnant = -1; //id du gagnant
int valeur;

typedef enum {
    STATE_RULES,
    STATE_LOBBY,
    STATE_INGAME,
    STATE_GAMEOVER,
    STATE_VOTE
} GameState;
GameState currentGameState = STATE_RULES;

char *nbobjets[]={"5","5","5","5","4","3","3","3"};
char *nbnoms[]={"Sebastian Moran", "irene Adler", "inspector Lestrade",
  "inspector Gregson", "inspector Baynes", "inspector Bradstreet",
  "inspector Hopkins", "Sherlock Holmes", "John Watson", "Mycroft Holmes",
  "Mrs. Hudson", "Mary Morstan", "James Moriarty"};

int joueurCourant;//quel joueurs doit jouer
int joueur_restant;//combien de joueurs ne sont pas éliminés
int eliminated[4];//liste des joueurs éliminés
char info[256];//pour texte divers
char info2[256];//pour texte divers
char pseudo[256]; //pour afficher le nom du joueur
volatile int synchro;
int showQuitConfirmation = 0;
int hasVoted = 0;

void *fn_serveur_tcp(void *arg)
{
        int sockfd, newsockfd, portno;
        socklen_t clilen;
        struct sockaddr_in serv_addr, cli_addr;
        int n;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd<0)
        {
                printf("sockfd error\n");
                exit(1);
        }

        bzero((char *) &serv_addr, sizeof(serv_addr));
        portno = gClientPort;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);
       if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        {
                printf("bind error\n");
                exit(1);
        }

        listen(sockfd,5);
        clilen = sizeof(cli_addr);
        while (1)
        {
                newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                if (newsockfd < 0)
                {
                        printf("accept error\n");
                        exit(1);
                }

                bzero(gbuffer,256);//on met dans gbuffer le message recu
                n = read(newsockfd,gbuffer,255);
                if (n < 0)
                {
                        printf("read error\n");
                        exit(1);
                }
                //printf("%s",gbuffer);

                synchro=1;

                while (synchro);

     }
}

void sendMessageToServer(char *ipAddress, int portno, char *mess)
{
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char sendbuffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server = gethostbyname(ipAddress);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        {
                printf("ERROR connecting\n");
                exit(1);
        }

        sprintf(sendbuffer,"%s\n",mess);
        n = write(sockfd,sendbuffer,strlen(sendbuffer));

    close(sockfd);
}

void renderText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color, bool center) {
    if (!text || strlen(text) == 0) return;
    SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, text, color);
    if (!surfaceMessage) {
        printf("TTF_RenderText_Solid Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
    SDL_Rect Message_rect;
    if (center)
	    Message_rect.x = x - surfaceMessage->w / 2;
    else
	    Message_rect.x = x;
    Message_rect.y = y;
    Message_rect.w = surfaceMessage->w;
    Message_rect.h = surfaceMessage->h;
    SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
    SDL_DestroyTexture(Message);
    SDL_FreeSurface(surfaceMessage);
}

void resetClientState() {
	joueurSel = -1;
	objetSel = -1;
	guiltSel = -1;
	gagnant = -1;
	coupable = -1;
	hasVoted = 0;
	joueurCourant = -1;
	goEnabled = 0;
	
	b[0] = -1; b[1] = -1; b[2] = -1;

	for (int i=0; i<13; i++) guiltGuess[i] = 0;
	for (int i=0; i<4; i++) {
		for (int j=0; j<8; j++) {
			tableCartes[i][j] = -1;
		}
		eliminated[i] = 0;
	}
}

int main(int argc, char ** argv)
{
	int ret;
	int i,j,n;

    int quit = 0;
    SDL_Event event;
	int mx,my;
	char sendBuffer[1024];
	char lname[256];
	int id;
	gId = -1;

        if (argc<6)
        {
                printf("<app> <Main server ip address> <Main server port> <Client ip address> <Client port> <player name>\n");
                exit(1);
        }

        strcpy(gServerIpAddress,argv[1]);
        gServerPort=atoi(argv[2]);
        strcpy(gClientIpAddress,argv[3]);
        gClientPort=atoi(argv[4]);
        strcpy(gName,argv[5]);

    SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();
 
    SDL_Window * window = SDL_CreateWindow("SDL2 SH13",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, 0);
 
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

    SDL_Surface *deck[13],*objet[8],*gobutton,*connectbutton,*logo, *quitbutton;

	deck[0] = IMG_Load("SH13_0.png");
	deck[1] = IMG_Load("SH13_1.png");
	deck[2] = IMG_Load("SH13_2.png");
	deck[3] = IMG_Load("SH13_3.png");
	deck[4] = IMG_Load("SH13_4.png");
	deck[5] = IMG_Load("SH13_5.png");
	deck[6] = IMG_Load("SH13_6.png");
	deck[7] = IMG_Load("SH13_7.png");
	deck[8] = IMG_Load("SH13_8.png");
	deck[9] = IMG_Load("SH13_9.png");
	deck[10] = IMG_Load("SH13_10.png");
	deck[11] = IMG_Load("SH13_11.png");
	deck[12] = IMG_Load("SH13_12.png");

	objet[0] = IMG_Load("SH13_pipe_120x120.png");
	objet[1] = IMG_Load("SH13_ampoule_120x120.png");
	objet[2] = IMG_Load("SH13_poing_120x120.png");
	objet[3] = IMG_Load("SH13_couronne_120x120.png");
	objet[4] = IMG_Load("SH13_carnet_120x120.png");
	objet[5] = IMG_Load("SH13_collier_120x120.png");
	objet[6] = IMG_Load("SH13_oeil_120x120.png");
	objet[7] = IMG_Load("SH13_crane_120x120.png");

	gobutton = IMG_Load("gobutton.png");
	connectbutton = IMG_Load("connectbutton.png");
	quitbutton = IMG_Load("quit.png");

	logo = IMG_Load("logo.png");//affichage

	strcpy(gNames[0],"-");
	strcpy(gNames[1],"-");
	strcpy(gNames[2],"-");
	strcpy(gNames[3],"-");

	joueurSel=-1;
	objetSel=-1;
	guiltSel=-1;

	b[0]=-1;//tableau de bit map ---> -1 pas d'image on n'affiche pas
	b[1]=-1;
	b[2]=-1;

	for (i=0;i<13;i++)
		guiltGuess[i]=0;

	for (i=0;i<4;i++)
		for (j=0;j<8;j++)
			tableCartes[i][j]=-1;

	goEnabled=0;
	connectEnabled=1;
//pas de surface dans la fnetres mais des textures, c'est pourquoi on change
    SDL_Texture *texture_deck[13],*texture_gobutton,*texture_connectbutton, *texture_logo,*texture_objet[8], *texture_quitbutton;

	for (i=0;i<13;i++)
		texture_deck[i] = SDL_CreateTextureFromSurface(renderer, deck[i]);
	for (i=0;i<8;i++)
		texture_objet[i] = SDL_CreateTextureFromSurface(renderer, objet[i]);

    texture_gobutton = SDL_CreateTextureFromSurface(renderer, gobutton);
    texture_connectbutton = SDL_CreateTextureFromSurface(renderer, connectbutton);
	texture_quitbutton = SDL_CreateTextureFromSurface(renderer, quitbutton);
	texture_logo = SDL_CreateTextureFromSurface(renderer, logo);

    TTF_Font* Sans = TTF_OpenFont("sans.ttf", 15); 
    printf("Sans=%p\n",Sans);

   /* Creation du thread serveur tcp. */
   printf ("Creation du thread serveur tcp !\n");
   synchro=0;
   ret = pthread_create ( & thread_serveur_tcp_id, NULL, fn_serveur_tcp, NULL);//creer le thread du serveur local

    sprintf(info2, "Bonjour %s que le jeu commence", gName);
	sprintf(info, " ");
	sprintf(pseudo, "joueur: %s", gName);
    while (!quit)//boucle graphique
    {
	if (SDL_PollEvent(&event))
	{
		//printf("un event\n");
        	switch (event.type)
        	{
            		case SDL_QUIT:
                		showQuitConfirmation = 1;
                		break;
			case  SDL_MOUSEBUTTONDOWN:
				SDL_GetMouseState( &mx, &my );//renvoie la position de la souris
				if (showQuitConfirmation) {
					// Popup is active, check for Oui/Non clicks
					// "Oui" button area:
					if (mx >= (1024/2 - 150) && mx <= (1024/2 - 50) && my >= (768/2 + 10) && my <= (768/2 + 60)) {
						quit = 1; // Yes, quit
						if (gId != -1) {
							sprintf(sendBuffer, "Q %d", gId);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
						}
					}
					// "Non" button area:
					if (mx >= (1024/2 + 50) && mx <= (1024/2 + 150) && my >= (768/2 + 10) && my <= (768/2 + 60)) {
						showQuitConfirmation = 0; // No, go back to game
					}
				} else if (currentGameState == STATE_VOTE && !hasVoted) {

					// "Oui" button for replay
					if (mx >= 1024/2 - 150 && mx <= 1024/2 - 50 && my >= 768/2 + 10 && my <= 768/2 + 60) {
						sprintf(sendBuffer, "Y %d", gId);
						sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
						hasVoted = 1;
					}
					// "Non" button for replay
					if (mx >= 1024/2 + 50 && mx <= 1024/2 + 150 && my >= 768/2 + 10 && my <= 768/2 + 60) {
						sprintf(sendBuffer, "N %d", gId);
						sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
						hasVoted = 1;
						quit = 1; // On ferme le jeu immédiatement
					}
				} else if (currentGameState == STATE_RULES) {
					// "Jouer" button on rules screen
					if (mx >= 1024/2 - 100 && mx <= 1024/2 + 100 && my >= 600 && my <= 650) {
						currentGameState = STATE_LOBBY;
						connectEnabled = 1;
					}
				} else {
					//printf("mx=%d my=%d\n",mx,my);
					if ((mx<200) && (my<50) && (connectEnabled==1))//pour jouer
					{//C pour connexion
						sprintf(sendBuffer,"C %s %d %s",gClientIpAddress,gClientPort,gName);
						sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
						printf("connect sent |%s|\n",sendBuffer);
						connectEnabled=0;
					}
					else if ((mx>=974) && (mx<=1014) && (my>=718) && (my<=758)) { // Bouton Quitter
						showQuitConfirmation = 1;
					}
					else if (currentGameState != STATE_LOBBY && (mx>=0) && (mx<200) && (my>=90) && (my<330))
					{
						joueurSel=(my-90)/60;
						guiltSel=-1;
					}
					else if ((mx>=200) && (mx<680) && (my>=0) && (my<90))
					{
						objetSel=(mx-200)/60;
						guiltSel=-1;
					}
					else if ((mx>=100) && (mx<250) && (my>=350) && (my<740))
					{
						joueurSel=-1;
						objetSel=-1;
						guiltSel=(my-350)/30;
					}
					else if ((mx>=250) && (mx<300) && (my>=350) && (my<740))
					{
						int ind=(my-350)/30;
						guiltGuess[ind]=1-guiltGuess[ind];
					}
					else if ((mx>=500) && (mx<700) && (my>=350) && (my<450) && (goEnabled==1))
					{
						printf("go! joueur=%d objet=%d guilt=%d\n",joueurSel, objetSel, guiltSel);
						if (guiltSel!=-1)
						{//choix coupable
							sprintf(sendBuffer,"G %d %d",gId, guiltSel);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							goEnabled = 0;
						}
						else if ((objetSel!=-1) && (joueurSel==-1))
						{//demande si tout le monde a cette carte
							sprintf(sendBuffer,"O %d %d",gId, objetSel);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							goEnabled = 0;
						}
						else if ((objetSel!=-1) && (joueurSel!=-1))
						{//demande a une personne si elle a cette carte
							sprintf(sendBuffer,"S %d %d %d",gId, joueurSel,objetSel);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							goEnabled = 0;
						}
					}
					else
					{
						joueurSel=-1;
						objetSel=-1;
						guiltSel=-1;
					}
				}
				break;
			case  SDL_MOUSEMOTION:
				SDL_GetMouseState( &mx, &my );
				break;
        	}
	}

        if (synchro==1)
        {
                printf("consomme %s",gbuffer);

		// Handle multi-character commands first
		if (strncmp(gbuffer, "CLOSE", 5) == 0) {
			printf("CLIENT: Processing 'CLOSE' command. Quitting.\n");
			quit = 1;
		}
		else {
		// Handle single-character commands
		switch (gbuffer[0])
		{
			// Message 'I' : le joueur recoit son Id
			case 'I':
				int id;// RAJOUTER DU CODE ICI
				sscanf(gbuffer, "I %d", &gId);//attention il faut bien que ce soit " et pas ' 
				printf("Id reçu : %d\n", gId);

				break;
			// Message 'L' : le joueur recoit la liste des joueurs
			case 'L':
				// RAJOUTER DU CODE ICI
				sscanf(gbuffer, "L %s %s %s %s", gNames[0], gNames[1], gNames[2], gNames[3]);
				sprintf(info, "En attente de joueurs ...");
				break;
			// Message 'D' : le joueur recoit ses trois cartes
			case 'D'://donc modifier le tableau b
				sprintf(info2, "Que le jeu commence !");//debut du jeu
				// RAJOUTER DU CODE ICI
				int c0, c1, c2;//numéro des cartes
				//on recupère les nb des 3 cartes, on convertie le texte en nombres
				if (sscanf(gbuffer, "D %d %d %d", &c0, &c1, &c2) == 3) {//verif qu'on a bien 3 cartes
					b[0] = c0;//premier carte et lien avec SDL
					b[1] = c1;//deuxième carte
					b[2] = c2;//troisième carte
					printf("Cartes reçues : %d %d %d\n", b[0], b[1], b[2]);
	
				} else {
					printf("Erreur lors de la reception des cartes\n");
				}

				break;
			// Message 'M' : le joueur recoit le n° du joueur courant
			// Cela permet d'affecter goEnabled pour autoriser l'affichage du bouton go
			case 'M':
				// RAJOUTER DU CODE ICI
				sscanf(gbuffer, "M %d", &joueurCourant);
				currentGameState = STATE_INGAME;
				if (joueurCourant == gId) {
					sprintf(info, "A vous de jouer ! Faites une action et cliquez sur GO.");
				}
				else {
					sprintf(info, "C'est au tour de %s de jouer...", gNames[joueurCourant]);
				}
				goEnabled = (joueurCourant == gId) ? 1 : 0;

				break;
			// Message 'V' : le joueur recoit une valeur de tableCartes
			case 'V':
				// RAJOUTER DU CODE ICI
				sscanf(gbuffer, "V %d %d %d", &i, &j, &n);
				if (tableCartes[i][j] == -1 || tableCartes[i][j] == 0 || tableCartes[i][j] == 100){
						tableCartes[i][j] = n;
				}

				break;
			case 'W':
				sscanf(gbuffer, "W %d %d", &i, &coupable);
				sprintf(info2, "%s a gagne! %s est coupable!", gNames[i], nbnoms[coupable]);
				goEnabled = 0;
				gagnant = i;
				currentGameState = STATE_GAMEOVER;

				break;
			case 'E':
				sprintf(info2, " ");//affiche rien
			    sscanf(gbuffer, "E %d %d", &i, &j);
			    sprintf(info2, "%s est elimine! %s est innocent!", gNames[i], nbnoms[j]);//affiche qui est eliminer est pourquoi
			    eliminated[i] = 1;
			    joueur_restant--;//un joueur de moins
				guiltGuess[j] = 1;

			    break;
			case 'K':
				{
					int quit_id;
					sscanf(gbuffer, "K %d", &quit_id);
					if (eliminated[quit_id] == 0) {
						joueur_restant--;
						eliminated[quit_id] = 1;
						sprintf(info2, "%s a quitte la partie.", gNames[quit_id]);
					}
				}
				break;
			case 'P': // Propose replay
				currentGameState = STATE_VOTE;
				hasVoted = 0;
				break;
			case 'R': // Reset for new game
				resetClientState();
				currentGameState = STATE_INGAME;
				break;
			case 'B': // Back to lobby
				resetClientState();
				currentGameState = STATE_LOBBY;
				sprintf(info, "En attente de nouveaux joueurs...");
				break;
		}
		}
		synchro=0;
        }

        SDL_Rect dstrect_grille = { 512-250, 10, 500, 350 };
        SDL_Rect dstrect_image = { 0, 0, 500, 330 };
        SDL_Rect dstrect_image1 = { 0, 340, 250, 330/2 };

	SDL_SetRenderDrawColor(renderer, 255, 230, 230, 230);
	SDL_Rect rect = {0, 0, 1024, 768}; 
	SDL_RenderFillRect(renderer, &rect);

	if (currentGameState == STATE_RULES) {
		SDL_Color col = {0, 0, 0};
		// Background box
		SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
		SDL_Rect rulesBg = {100, 100, 1024 - 200, 768 - 250};
		SDL_RenderFillRect(renderer, &rulesBg);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderDrawRect(renderer, &rulesBg);

		// Title
		renderText(renderer, Sans, "Regles du jeu Sherlock 13", 1024/2, 120, col, true);

		// Rules text
		renderText(renderer, Sans, "Le but est de trouver le coupable parmi les 13 suspects.", 120, 180, col, false);
		renderText(renderer, Sans, "Chaque joueur a 3 cartes. La 13eme carte, mise de cote, est le coupable.", 120, 210, col, false);
		renderText(renderer, Sans, "A votre tour, vous pouvez effectuer UNE action :", 120, 250, col, false);
		renderText(renderer, Sans, "1. Interroger tout le monde : 'Qui a cet indice ?' (clic sur un indice en haut).", 140, 280, col, false);
		renderText(renderer, Sans, "2. Interroger un joueur : 'Combien as-tu de cartes avec cet indice ?' (clic sur un joueur, puis un indice).", 140, 310, col, false);
		renderText(renderer, Sans, "3. Porter une accusation : 'Je pense que le coupable est...' (clic sur un suspect en bas a gauche).", 140, 340, col, false);
		renderText(renderer, Sans, "Une fausse accusation vous elimine de la partie !", 120, 380, col, false);
		renderText(renderer, Sans, "Le dernier joueur en lice ou le premier a trouver le coupable gagne.", 120, 410, col, false);

		// "Jouer" button
		SDL_Rect playButton = {1024/2 - 100, 600, 200, 50};
		SDL_SetRenderDrawColor(renderer, 100, 220, 100, 255);
		SDL_RenderFillRect(renderer, &playButton);
		renderText(renderer, Sans, "Jouer !", 1024/2, 615, col, true);

	} else { // Game display (Lobby, In-Game, Game Over)

	//logo in lobby
	if (currentGameState == STATE_LOBBY) {
		SDL_Rect dstrect_logo = {320, 250, 400, 400};
		SDL_RenderCopy(renderer, texture_logo, NULL, &dstrect_logo);
	}

	if (joueurSel!=-1)
	{
		SDL_SetRenderDrawColor(renderer, 255, 180, 180, 255);
		SDL_Rect rect1 = {0, 90+joueurSel*60, 200 , 60}; 
		SDL_RenderFillRect(renderer, &rect1);
	}

	if (objetSel!=-1)
	{
		SDL_SetRenderDrawColor(renderer, 180, 255, 180, 255);
		SDL_Rect rect1 = {200+objetSel*60, 0, 60 , 90}; 
		SDL_RenderFillRect(renderer, &rect1);
	}

	if (guiltSel!=-1)
	{
		SDL_SetRenderDrawColor(renderer, 180, 180, 255, 255);
		SDL_Rect rect1 = {100, 350+guiltSel*30, 150 , 30}; 
		SDL_RenderFillRect(renderer, &rect1);
	}

	// Affichage des icones des objets
	for (i = 0; i < 8; i++) {
		SDL_Rect dstrect_objet = { 210 + i * 60, 10, 40, 40 };
		SDL_RenderCopy(renderer, texture_objet[i], NULL, &dstrect_objet);
	}

    // Affichage des comptes des objets
	SDL_Color col1 = {0, 0, 0};
	for (i=0;i<8;i++)
	{
		renderText(renderer, Sans, nbobjets[i], 230 + i * 60, 50, col1, true);
	}

        for (i=0;i<13;i++)
        {
                SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, nbnoms[i], col1);
                SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

                SDL_Rect Message_rect;
                Message_rect.x = 105;
                Message_rect.y = 350+i*30;
                Message_rect.w = surfaceMessage->w;
                Message_rect.h = surfaceMessage->h;

                SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
                SDL_DestroyTexture(Message);
                SDL_FreeSurface(surfaceMessage);
        }

	for (i=0;i<4;i++)
        	for (j=0;j<8;j++)
        	{
			if (tableCartes[i][j]!=-1)
			{
				char mess[10];
				if (tableCartes[i][j]==100)
					sprintf(mess,"*");
				else
					sprintf(mess,"%d",tableCartes[i][j]);
                		SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, mess, col1);
                		SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

                		SDL_Rect Message_rect;
                		Message_rect.x = 230+j*60;
                		Message_rect.y = 110+i*60;
                		Message_rect.w = surfaceMessage->w;
                		Message_rect.h = surfaceMessage->h;

                		SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
                		SDL_DestroyTexture(Message);
                		SDL_FreeSurface(surfaceMessage);
			}
        	}


	// Sebastian Moran
	{
        SDL_Rect dstrect_crane = { 0, 350, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[7], NULL, &dstrect_crane);
	}
	{
        SDL_Rect dstrect_poing = { 30, 350, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
	}
	// Irene Adler
	{
        SDL_Rect dstrect_crane = { 0, 380, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[7], NULL, &dstrect_crane);
	}
	{
        SDL_Rect dstrect_ampoule = { 30, 380, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
	}
	{
        SDL_Rect dstrect_collier = { 60, 380, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[5], NULL, &dstrect_collier);
	}
	// Inspector Lestrade
	{
        SDL_Rect dstrect_couronne = { 0, 410, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
	}
	{
        SDL_Rect dstrect_oeil = { 30, 410, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[6], NULL, &dstrect_oeil);
	}
	{
        SDL_Rect dstrect_carnet = { 60, 410, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
	}
	// Inspector Gregson 
	{
        SDL_Rect dstrect_couronne = { 0, 440, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
	}
	{
        SDL_Rect dstrect_poing = { 30, 440, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
	}
	{
        SDL_Rect dstrect_carnet = { 60, 440, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
	}
	// Inspector Baynes 
	{
        SDL_Rect dstrect_couronne = { 0, 470, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
	}
	{
        SDL_Rect dstrect_ampoule = { 30, 470, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
	}
	// Inspector Bradstreet
	{
        SDL_Rect dstrect_couronne = { 0, 500, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
	}
	{
        SDL_Rect dstrect_poing = { 30, 500, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
	}
	// Inspector Hopkins 
	{
        SDL_Rect dstrect_couronne = { 0, 530, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
	}
	{
        SDL_Rect dstrect_pipe = { 30, 530, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
	}
	{
        SDL_Rect dstrect_oeil = { 60, 530, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[6], NULL, &dstrect_oeil);
	}
	// Sherlock Holmes 
	{
        SDL_Rect dstrect_pipe = { 0, 560, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
	}
	{
        SDL_Rect dstrect_ampoule = { 30, 560, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
	}
	{
        SDL_Rect dstrect_poing = { 60, 560, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
	}
	// John Watson 
	{
        SDL_Rect dstrect_pipe = { 0, 590, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
	}
	{
        SDL_Rect dstrect_oeil = { 30, 590, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[6], NULL, &dstrect_oeil);
	}
	{
        SDL_Rect dstrect_poing = { 60, 590, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
	}
	// Mycroft Holmes
	{
        SDL_Rect dstrect_pipe = { 0, 620, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
	}
	{
        SDL_Rect dstrect_ampoule = { 30, 620, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
	}
	{
        SDL_Rect dstrect_carnet = { 60, 620, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
	}
	// Mrs. Hudson
	{
        SDL_Rect dstrect_pipe = { 0, 650, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
	}
	{
        SDL_Rect dstrect_collier = { 30, 650, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[5], NULL, &dstrect_collier);
	}
	// Mary Morstan
	{
        SDL_Rect dstrect_carnet = { 0, 680, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
	}
	{
        SDL_Rect dstrect_collier = { 30, 680, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[5], NULL, &dstrect_collier);
	}
	// James Moriarty
	{
        SDL_Rect dstrect_crane = { 0, 710, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[7], NULL, &dstrect_crane);
	}
	{
        SDL_Rect dstrect_ampoule = { 30, 710, 30, 30 };
        SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
	}

	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

	// Afficher les suppositions
	for (i=0;i<13;i++)
		if (guiltGuess[i])
		{
			SDL_RenderDrawLine(renderer, 250,350+i*30,300,380+i*30);
			SDL_RenderDrawLine(renderer, 250,380+i*30,300,350+i*30);
		}

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderDrawLine(renderer, 0,30+60,680,30+60);
	SDL_RenderDrawLine(renderer, 0,30+120,680,30+120);
	SDL_RenderDrawLine(renderer, 0,30+180,680,30+180);
	SDL_RenderDrawLine(renderer, 0,30+240,680,30+240);
	SDL_RenderDrawLine(renderer, 0,30+300,680,30+300);

	SDL_RenderDrawLine(renderer, 200,0,200,330);
	SDL_RenderDrawLine(renderer, 260,0,260,330);
	SDL_RenderDrawLine(renderer, 320,0,320,330);
	SDL_RenderDrawLine(renderer, 380,0,380,330);
	SDL_RenderDrawLine(renderer, 440,0,440,330);
	SDL_RenderDrawLine(renderer, 500,0,500,330);
	SDL_RenderDrawLine(renderer, 560,0,560,330);
	SDL_RenderDrawLine(renderer, 620,0,620,330);
	SDL_RenderDrawLine(renderer, 680,0,680,330);

	for (i=0;i<14;i++)
		SDL_RenderDrawLine(renderer, 0,350+i*30,300,350+i*30);
		SDL_RenderDrawLine(renderer, 100,350,100,740);
		SDL_RenderDrawLine(renderer, 250,350,250,740);
		SDL_RenderDrawLine(renderer, 300,350,300,740);

	

        //SDL_RenderCopy(renderer, texture_grille, NULL, &dstrect_grille);
	if (b[0]!=-1)
	{
        	SDL_Rect dstrect = { 750, 0, 1000/4, 660/4 };
        	SDL_RenderCopy(renderer, texture_deck[b[0]], NULL, &dstrect);
	}
	if (b[1]!=-1)
	{
        	SDL_Rect dstrect = { 750, 200, 1000/4, 660/4 };
        	SDL_RenderCopy(renderer, texture_deck[b[1]], NULL, &dstrect);
	}
	if (b[2]!=-1)
	{
        	SDL_Rect dstrect = { 750, 400, 1000/4, 660/4 };
        	SDL_RenderCopy(renderer, texture_deck[b[2]], NULL, &dstrect);
	}

	// Le bouton connect
	if (connectEnabled==1)
	{
        	SDL_Rect dstrect = { 0, 0, 200, 50 };
        	SDL_RenderCopy(renderer, texture_connectbutton, NULL, &dstrect);
	}
	// Le bouton go
	if (goEnabled==1)
	{
        	SDL_Rect dstrect = { 530, 350, 200, 150 };
        	SDL_RenderCopy(renderer, texture_gobutton, NULL, &dstrect);
	}

	SDL_Color col = {0, 0, 0};

	// Affichage des joueurs et de leur statut
	if (currentGameState == STATE_INGAME)
	{
		SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255);
		SDL_Rect rectJoueurCourant = {0, 90 + joueurCourant * 60, 200, 60};
		SDL_RenderFillRect(renderer, &rectJoueurCourant);
	}
	if (gagnant != -1)
	{
		SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
		SDL_Rect rectJoueurgagnant = {0, 90 + gagnant * 60, 200, 60};
		SDL_RenderFillRect(renderer, &rectJoueurgagnant);
	}
	for (i=0; i<4; i++)
	{
		if (eliminated[i] == 1)
		{
			SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
			SDL_Rect rectJoueurElimine = {0, 90 + i * 60, 200, 60};
			SDL_RenderFillRect(renderer, &rectJoueurElimine);
		}
	}

	// Noms des joueurs
	for (i=0;i<4;i++) {
		renderText(renderer, Sans, gNames[i], 10, 110+i*60, col, false);
    }

	// Bandeaux d'informations
	renderText(renderer, Sans, info, 400, 700, col, false);
	renderText(renderer, Sans, info2, 400, 650, col, false);

	// Affichage du nom du joueur
	renderText(renderer, Sans, pseudo, 50, 50, col, false);

	// Bouton Quitter
	SDL_Rect quitButtonRect = {974, 718, 40, 40};
	SDL_RenderCopy(renderer, texture_quitbutton, NULL, &quitButtonRect);

	// Popup de confirmation pour quitter
	if (showQuitConfirmation) {
		// Fond assombri
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
		SDL_Rect screenRect = {0, 0, 1024, 768};
		SDL_RenderFillRect(renderer, &screenRect);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

		// Boîte de dialogue
		SDL_Rect popupRect = {1024/2 - 250, 768/2 - 75, 500, 150};
		SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
		SDL_RenderFillRect(renderer, &popupRect);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderDrawRect(renderer, &popupRect);

		renderText(renderer, Sans, "Voulez-vous vraiment quitter ?", 1024/2, 768/2 - 45, col, true);

		SDL_Rect yesRect = {1024/2 - 150, 768/2 + 10, 100, 50};
		SDL_SetRenderDrawColor(renderer, 100, 220, 100, 255);
		SDL_RenderFillRect(renderer, &yesRect);
		renderText(renderer, Sans, "Oui", 1024/2 - 100, 768/2 + 25, col, true);

		SDL_Rect noRect = {1024/2 + 50, 768/2 + 10, 100, 50};
		SDL_SetRenderDrawColor(renderer, 220, 100, 100, 255);
		SDL_RenderFillRect(renderer, &noRect);
		renderText(renderer, Sans, "Non", 1024/2 + 100, 768/2 + 25, col, true);
	}

	if (currentGameState == STATE_VOTE) {
		// Fond assombri
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
		SDL_Rect screenRect = {0, 0, 1024, 768};
		SDL_RenderFillRect(renderer, &screenRect);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

		// Boîte de dialogue
		SDL_Rect popupRect = {1024/2 - 250, 768/2 - 75, 500, 150};
		SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
		SDL_RenderFillRect(renderer, &popupRect);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderDrawRect(renderer, &popupRect);

		renderText(renderer, Sans, "Voulez-vous rejouer ?", 1024/2, 768/2 - 45, col, true);

		if (!hasVoted) {
			SDL_Rect yesRect = {1024/2 - 150, 768/2 + 10, 100, 50};
			SDL_SetRenderDrawColor(renderer, 100, 220, 100, 255);
			SDL_RenderFillRect(renderer, &yesRect);
			renderText(renderer, Sans, "Oui", 1024/2 - 100, 768/2 + 25, col, true);

			SDL_Rect noRect = {1024/2 + 50, 768/2 + 10, 100, 50};
			SDL_SetRenderDrawColor(renderer, 220, 100, 100, 255);
			SDL_RenderFillRect(renderer, &noRect);
			renderText(renderer, Sans, "Non", 1024/2 + 100, 768/2 + 25, col, true);
		} else {
			renderText(renderer, Sans, "En attente des autres joueurs...", 1024/2, 768/2 + 25, col, true);
		}
	}

	} // End of game display
		// On affiche le rendu final une seule fois par tour de boucle
        SDL_RenderPresent(renderer);

    }
 
    SDL_DestroyTexture(texture_deck[0]);
    SDL_DestroyTexture(texture_deck[1]);
    SDL_DestroyTexture(texture_quitbutton);
    SDL_FreeSurface(quitbutton);
    SDL_FreeSurface(deck[0]);
    SDL_FreeSurface(deck[1]);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
 
    SDL_Quit();
 
    return 0;
}
