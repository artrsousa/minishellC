#include <errno.h>
#include <stdio.h> 
#include <stdlib.h>     	
#include <unistd.h> 		
#include <string.h>			
#include <signal.h>
#include <pthread.h>			  	
#include <sys/wait.h>
#include <sys/types.h>		 
//COMMIT_FINAL

#define MAXSIZE 1024  
#define TRUE 1
#define FALSE 0
                                       
char shorthostname[255];
char pwd[2048];
int schar;                  //position of |, >, <
struct sigaction act;
int size;

void getCommand  (char command[], int *size, int *numParametros);                     
void splitLine   (char command[], char argv[][MAXSIZE], int *size); 
void getSignal   (int sig);
void parseTwo    (char* arg[], int* numParametros, char* befCommand[], char* aftCommand[]);
void* searchChar (void* arg[]);

int  main(void) {
	pid_t pid;
	int status;

	// FAZ COM QUE O PAI IGNORE O SIGINT, POIS É O FILHO QUE DEVE TRATAR // 
	(void) signal(SIGINT, SIG_IGN);
	
	while (TRUE) {			
		size = 0;
		schar = 0;
		char command[MAXSIZE];
		int numParametros = 0;
		
		// DEFININDO ROTINA PARA TRATAMENTO DE SINAL // 
		act.sa_handler = getSignal;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		sigaction(SIGINT, &act, NULL);
		 
		// RECEBE O COMANDO DO USUÁRIO // 
		getCommand (command, &size, &numParametros);
		char argv[numParametros][MAXSIZE];
		splitLine (command, argv, &size);

		char *cmd = argv[0];
		char *arg[numParametros+1]; 
		arg[numParametros] = NULL;							

		// CONVERSÃO DE *CHAR[][MAXSIZE] PARA *CHAR[] //
		for (int k = 0; k < numParametros; ++k) 
			arg[k] = argv[k]; 

		// IDENTIFICA SE UM PROCESSO DEVE SER EXECUTADO EM BACKGROUND //
		char* lastString = arg[numParametros-1];
		if (strcmp(lastString, "&") == 0)
			arg[numParametros-1] = '\0';
		
		// SE USUARIO DIGITAR EXIT OU QUIT ENCERRAR MINI-SHELL // 
		if ((strcmp(cmd,"exit") == 0) || (strcmp(cmd,"quit") == 0)) { exit(EXIT_FAILURE); }

		// MUDA O DIRETORIO CORRENTE //
		if ((strcmp(cmd, "cd") == 0)) {  
			if (chdir(arg[1]) == -1)
				printf("No such file or directory \n");
			else {;}
		} else {	
			// SEPARA A LINHA EM 2 CASO TENHA ALGUM SCHAR //		
			int size2 = numParametros - schar - 1;
			char* befCommand[schar + 1];
			char* aftCommand[size2 + 1];
			
			parseTwo (arg, &numParametros, befCommand, aftCommand);
			char *cmdp  = befCommand[0];
			char *cmdp2 = aftCommand[0];
			befCommand[schar] = NULL;
			aftCommand[size2] = NULL;
			
			//INICIALIZA O PIPE EM UM ARRAY DE INT QUE SERÁ UTILIZADO POR 2 PROCESSOS //
			//FD[0] APENAS LEITURA, FD[1] ESCRITA // 
			int fd[2];
			if (pipe(fd) < 0)
				printf("pipe couldn't be initialized. %s\n", strerror(errno));
				 
			pid_t process = fork();
			
			if (process == 0) {
				if (schar != 0) {
					if (strcmp(arg[schar], "|") == 0) {
						//MUDA A SAIDA PADRAO COM O DUP2 //
						//PERMITE APENAS A ESCRITA NO PIPE//
						close(fd[0]);
						dup2(fd[1], STDOUT_FILENO);
						close(fd[1]);

						if ((execvp(cmdp, befCommand) == -1)) {	
							printf ("fail command. %s\n", strerror(errno)); 
							kill(getpid(), 9);
						}
					} else {
						if (strcmp(arg[schar], "<") == 0) {
							//MUDAR A ENTRADA PADRAO PARA UM ARQUIVO //
							//ABRE OU CRIA ARQUIVO CASO NECESSARIO // 
							FILE* file = fopen(aftCommand[0], "r");
							int fdr = fileno(file);
							dup2(fdr, STDIN_FILENO);
							close(fdr);
					
							// EXECUTA O PROCESSO //
							if ((execvp(cmdp, befCommand) == -1)) {	
								printf ("fail command. %s\n", strerror(errno)); 
								kill(getpid(), 9);
							}
						} else {
							if ((strcmp(arg[schar], ">") == 0)) {
								//MUDAR A SAIDA PADRÃO PARA UM ARQUIVO //
								//ABRE OU CRIA ARQUIVO CASO NECESSÁRIO //
								//DIRECIONA A SAIDA E O ERRO PARA O ARQUIVO//
								FILE* file = fopen(aftCommand[0], "w");
								int fdr = fileno(file);
								dup2(fdr, STDOUT_FILENO);
								dup2(fdr, STDERR_FILENO);
								close(fdr);
						
								// EXECUTA PROCESSO //
								if ((execvp(cmdp, befCommand) == -1)) {	
									printf ("fail command. %s\n", strerror(errno)); 
									kill(getpid(), 9);
								}
							}
						}
					}
				} else {
					// PROCESSO EXECUTARA INDEPENDENTEMENTE DO PAI // 
					if (strcmp(lastString, "&") == 0)
						signal (SIGHUP, SIG_IGN);
			
					// EXECUTA O COMANDO DIGITADO PELO USUARIO // 
					if ((execvp(cmd, arg) == -1)) {	
						printf ("fail command\n"); 
						kill(getpid(), 9);
					}
				}
			} else if (process > 0) {
				 do {
					// SE NAO ENCONTRADO & ENTÃO AGUARDA O FILHO // 
					// SE IDENTIFICADO '&' NÃO ESPERAR O FILHO  // 
					if (!strcmp(lastString, "&") == 0)
						pid = waitpid(process, &status, 0);
						
					if (pid == -1) { exit(EXIT_FAILURE); }

					if (strcmp(arg[schar], "|") == 0) {
						pid_t new;
						pid_t newChild = fork();
						
						// CRIA NOVO PROCESSO PARA EXECUCAO COM PIPE
						if (newChild < 0)
							printf("Couldn't fork the second process\n");
						
						if (newChild == 0) {
							// MUDA A ENTRADA PADRAO COM O DUP2 // 
							// PERMITE APENAS A LEITURA NO PIPE // 
							close(fd[1]);
							dup2(fd[0], STDIN_FILENO);
							close(fd[0]);
						
							if ((execvp(cmdp2, aftCommand) == -1)) {	
								printf ("fail command\n"); 
								kill(getpid(), 9);
							}
						} else {
							// FECHA DESCRITORES E ESPERA // 
							int newStatus;
							close(fd[1]);
							close(fd[0]);
							new = waitpid(newChild, &newStatus, 0);
							if (new == -1) 
								exit(EXIT_FAILURE);
						}
					}
					if (WIFEXITED(status)) {
						// SE USUARIO DIGITOU EXIT ENTÃO SAIR // 
						if (WEXITSTATUS(status) == 1) {
							printf("exiting..., status=%d\n", WEXITSTATUS(status));
							exit(EXIT_SUCCESS);
						}
					} else  if (WIFSIGNALED(status)) {
							// SE FILHO ENVIOU SINAL, ENTAO EXIBE //
							printf("signaled... , status=%d\n", WTERMSIG(status));						
							if (status == 2) {exit(EXIT_SUCCESS);}	
					}
				} while (!WIFEXITED(status) && !WIFSIGNALED(status));		
			}	 
		}			
	}
}

void getCommand (char command[], int *size, int *numParametros) {
	int line = 0;
	char aux;
	
	// IMPRIME O HOST E USUARIO NO INICIO DA LINHA //
	gethostname(shorthostname, 255);
	getcwd(pwd, 2048);
	printf("%s@%s$:%s$ ", getenv("USER"), shorthostname, pwd);

	// LE COMANDO CHAR POR CHAR //
	while (aux != '\n') {
		aux = getchar();
		if (aux ==  32) ++*numParametros;
		command[line] = aux;
		++line;		
	}	
	command[line] = '\0';
	*size = line;
	++*numParametros;
}

void splitLine (char command[], char argv[][MAXSIZE], int *size) {
	int split = 0;
	int count = 0;
	pthread_t threadLine;
	
	// THREAD PARA PERCORRER O VETOR EM BUSCA DO SCHAR //
	int status = pthread_create(&threadLine, NULL, (void*)searchChar, (void*)command);
	if (status != 0) printf("thread couldn't be initialized\n");
	
	// PERCORRE O VETOR ARMAZENANDO CADA PARAMETRO EM UMA POSICAO //
	for (int j = 0; j < *size; ++j ) {
		if (command[j] == 32 || command[j] == '\n') {
			int sizeParam = j - split;
			char parametro[sizeParam];
			parametro [sizeParam] = '\0';
			
			for (int k = 0; k < sizeParam; ++k) {
				parametro[k] = command[split];
				++split;
			}
			split = j + 1;
			strcpy(argv[count], parametro); 
			++count; 
		}
	}
	
	//ESPERA O TERMINO DA EXECUCAO DA THREAD // 
	pthread_join(threadLine, NULL);
}

void parseTwo (char *arg[], int *numParametros, char *befCommand[], char *aftCommand[]) {
	int aux = 0;
	
	// SEPARA O VETOR DE CHAR EM 2 CASO TENHA ALGUM SCHAR //
	for (int k = 0; k < *numParametros; ++k) {
		if (k < schar) 
			befCommand[k] = arg[k];
		else if (k > schar) {
			aftCommand[aux] = arg[k];
			++aux;
		}
	}
	befCommand[schar] = '\0';
	aftCommand[aux]   = '\0';
}

void* searchChar (void* arg[]) {
	int check    = FALSE;
	int numParam = 0;
	char* arg2 = (char*) arg;
	
	// PERCORRE O VETOR IDENTIFICANDO O SCHAR PRESENTE //
	for (int i = 0; (i < size) && check == FALSE; ++i) {
		if (arg2[i] == 32 || arg2[i] == '\n')
			++numParam;
			
		if (arg2[i] == 124 || arg2[i] == 62 || arg2[i] == 60) {
			schar = numParam;
			check = TRUE;
		}
	}
}

void getSignal (int sig) {
	char check = 'n';

	// ROTINA PARA TRATAMENTO DE SINAL //
	printf("\n Are you sure?(y/n)\n I got signal %d : ", sig);
	check = getchar();

	if (check == 121) {
		printf("you sayed yes...\n");

		// DEFINE O HANDLER COMO DEFAULT E ENVIA O SINAL 2 QUE NÃO SERÁ IGNORADO //
		act.sa_handler = SIG_DFL;
		sigaction(SIGINT, &act, NULL);
		kill(getpid(), 2);
	} else {
		if(check == 110) {
			printf("you sayed no...\n");

			// RESTARTA A FLAG MANTENDO O HANDLER MANTENDO A ROTINA //
			act.sa_handler = getSignal;
			act.sa_flags = SA_RESTART;
			sigaction(SIGINT, &act, NULL);
		}
	}
}


