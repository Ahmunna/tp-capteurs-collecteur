/* Cette constante permet d’utiliser les versions "thread safe" des */
/* fonction de la lib C elle est OBLIGATOIRE */
#define _REENTRANT

#define DEBUG

#include <algorithm>
#include <cstring>
#include <dirent.h> 
#include <experimental/filesystem>
#include <iostream>
#include <fstream>
#include <mutex>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <vector>
#include <unistd.h>

#include "net_aux.h"

#define BUFFERMAX 100
//#define BIND_ADDR "127.0.0.1"
#define CHECK_INTERVAL 6000000
#define DATA_NOTIFIER 5
// Event names
#define EVT_DATA_AVAILABLE "dataAvailable"
#define EVT_DATA_AVAILABLE_ACK "dataAvailableAck"
#define EVT_GET_NODES "getNodes"
#define EVT_GET_NODES_END "endNodeList"
#define EVT_GET_DATA "getDataFile"
#define EVT_GET_DATA_END "getDataFileEnd"
#define EVT_GET_MIN_CONFIG "getMinConfig"
#define EVT_GET_ECO_CONFIG "getEcoConfig"
#define EVT_GET_FULL_CONFIG "getFullConfig"
#define MIN_CONFIG "min_config"
#define ECO_CONFIG "eco_config"
#define FULL_CONFIG "full_config"
typedef struct{
	int sock;
	char ip_addr[15];
} server_params;

int srv_sock;
std::string ip_addr_df("127.0.0.1"); //adresse par defaut
int port = 8002; // Port par defaut
int sink_port = 8000;
std::vector<std::string> nodeList;
std::vector<std::string> dataList;
std::vector<std::string> nodeswithdata;
std::string dataDirectory = "database/";
int numeroData = 1;
int T = 2;
int N = 20;
int dirData = 0;
int batterySize = 1000;
long lostFiles = 0;
long sentFiles = 0;

std::mutex nodeListMutex, dataListMutex, nodeswithdataMutex;

/* A server instance answering client commands */
void* serverFunc(void *s) {
	
	//server_params params = *((server_params*)(&s));
	server_params* params = (server_params*) s;
	int socket = params->sock;
	std::string ip_client(params->ip_addr);
	
	std::cout << "[SERVER] Recieving message from ip : " << ip_client.c_str() << std::endl;
	
	//int socket = *((int*)(&s));
	char buf[BUFFERMAX];

	// Reading command
	sock_receive(socket, buf, BUFFERMAX);

	std::cout << "[SERVER] Receiving message: " << buf << std::endl;

	
	if (message_is(buf, EVT_GET_DATA)) {
		dataListMutex.lock();
		for (std::string str : dataList) {
		if(batterySize < 20) {std::cout<<"[INFO] Not enough battery size"<<std::endl; 	close_connection(socket); pthread_exit(NULL);}
		std::cout<<"[INFO] consumming 20 battery enery"<<std::endl;
		batterySize -= 20;
		std::cout<<"[INFO] Battery size : "<< batterySize<<std::endl;
		if(batterySize < 50) {
		lostFiles += dataList.size();
		std::cout<<"[INFO] Lost files : " + dataList.size()<<std::endl;
		}
		sock_send(socket, str.c_str());
		sentFiles++;
		}
		sock_send(socket, EVT_GET_DATA_END);
		
		for(auto dataName: dataList) {
			std::string fileName = dataDirectory + "/" + dataName;
			remove(fileName.c_str());
		}
		//EFFACER LES DONNEES EXISTANTES
		dataList.clear();
		
		dataListMutex.unlock();
	}

	// Closing connection
	close_connection(socket);

	pthread_exit(NULL);
}

/* Le thread d'ecoute */
void* listenServer(void *n) {
	/* création de la socket de communication */
	srv_sock = create_socket();

	/* initialisation de la structure representant l'adresse */
	start_server(srv_sock, ip_addr_df.c_str(), port);

	while(1){
			
		//server_params* params;
		server_params* params = (server_params*)malloc(sizeof(server_params));
		char buf[BUFFERMAX];
		
		/* Attendre les requêtes de connexion */
		int sock_effective = wait_connection_adr(srv_sock, buf);
		
		//memset(params,0,sizeof(server_params));
		params->sock = sock_effective;
		strncpy(params->ip_addr, buf, strlen(buf));

		pthread_t thrd;
		if (pthread_create(&thrd, NULL, serverFunc, (void *) params) != 0) {
			std::cerr << "[SERVER] Error while creating a new thread" << std::endl;
			pthread_exit(NULL);
		}
	}
}


/* Mise a jour de la liste des nodes */
void* notifyDataAvailable(void *i) {
	char* ip_serveur = *((char**)(&i));
	char buf[BUFFERMAX];  /* buffer pour les données reçues*/

	/* Création de la socket de communication */
	int clt_sock = create_socket();

	/* demande d'une connexion au serveur */
	open_connection(clt_sock, ip_serveur, sink_port);
	
	std::string delimiter(".");
	std::string message = EVT_DATA_AVAILABLE + delimiter + ip_addr_df;
	if(batterySize < 10) {std::cout<<"[INFO] Not enough battery size"<<std::endl; close_connection(clt_sock);}
	std::cout<<"[INFO] consumming 10 battery enery"<<std::endl;
	batterySize -= 10;
	std::cout<<"[INFO] Battery size : "<< batterySize<<std::endl;
	if(batterySize < 50) {
		lostFiles += dataList.size();
		std::cout<<"[INFO] Lost files : " + dataList.size()<<std::endl;
	}
	/* envoi du message au serveur */
	sock_send(clt_sock, message.c_str());
	/* Lecture du message du client */
	while(true) {
		sock_receive(clt_sock, buf, BUFFERMAX);

		if (message_is(buf, EVT_DATA_AVAILABLE_ACK)) { break; }
	}

	/* fermeture de la socket */
	close_connection(clt_sock);
}

/* Battery size sensor */
void* getConfigFiles(void* i)
{

	char* ip_serveur = *((char**)(&i));
	char buf[BUFFERMAX];  /* buffer pour les données reçues*/

	/* Création de la socket de communication */
	int clt_sock = create_socket();

	/* demande d'une connexion au serveur */
	open_connection(clt_sock, ip_serveur, sink_port);
	
	std::string delimiter(".");
	std::string message ;

	if(batterySize < 300) message =  EVT_GET_MIN_CONFIG + delimiter + ip_addr_df;
	else if(batterySize < 600) message =  EVT_GET_ECO_CONFIG + delimiter + ip_addr_df;

	if(batterySize < 30) {std::cout<<"[INFO] Not enough battery size"<<std::endl; close_connection(clt_sock);}
	std::cout<<"[INFO] consumming 30 battery enery"<<std::endl;
	batterySize -= 30;
	std::cout<<"[INFO] Battery size : "<< batterySize<<std::endl;
	if(batterySize < 50) {
		lostFiles += dataList.size();
		std::cout<<"[INFO] Lost files : " + dataList.size()<<std::endl;
	}
	sock_send(clt_sock, message.c_str());
	sock_receive(clt_sock, buf, BUFFERMAX);

	std::string s(buf);
	std::cout<<"[INFO] Received " + s + " configuration file ! "<<std::endl;
	close_connection(clt_sock);
}

/* Client principal */
void* clientFunc(void *n) {
	while(1) {
		//sleep(T);
		//créer la prochaine data
		FILE* datafile;
		std::string num = std::to_string(numeroData);
		std::string filename(dataDirectory + "/" + ip_addr_df + "_data_" + num);
	    datafile = fopen(filename.c_str(), "w");
		fclose(datafile);
		
		//nodeswithdataMutex.lock();
		dataListMutex.lock();
		std::cout << "[CLIENT] Adding data " << filename.c_str() << std::endl;
		dataList.push_back(ip_addr_df + "_data_" + num);
		if(batterySize < 50) {std::cout<<"[INFO] Not enough battery size"<<std::endl; break;}
		std::cout<<"[INFO] consumming 50 battery enery"<<std::endl;
		batterySize -= 50;
		std::cout<<"[INFO] Battery size : "<< batterySize<<std::endl;
		if(batterySize < 50) {
			lostFiles += dataList.size();
			std::cout<<"[INFO] Lost files : " + dataList.size()<<std::endl;
		}
		numeroData++;
		dirData++;
		dataListMutex.unlock();
		pthread_t sensor; 
		if (pthread_create(&sensor, NULL, getConfigFiles, (void*) dataList.at(0).c_str()) != 0) {
		std::cerr << "[MAIN] Error while creating a new thread: server" << std::endl;
		pthread_exit(NULL);
		}
		std::cout << "[CLIENT_Sensor] List of current data in database " << std::endl;	
		for (std::string dataname : dataList) {
		std::cout << "[CLIENT_Sensor] " << dataname << std::endl;	
		}
		
		if(dirData%DATA_NOTIFIER == 0)
		{
			pthread_t traitement;
			if(pthread_create(&traitement, NULL, notifyDataAvailable, (void *) nodeList.at(0).c_str()) != 0) {
			std::cerr << "[CLIENT] Error while creating a new thread: client sensor data update" << std::endl;
			pthread_exit(NULL);
		}
		
		}
	usleep(CHECK_INTERVAL);	
	}
}

// Procédure principale
int main(int argc, char* argv[]) {
	// Traitement des arguments
	if (argc == 0) { return -1; }

	for (int i = 0; i < argc; i++) {
		std::string strarg(argv[i]);

		if(strarg == "-a") {
			if (i < (argc - 1)) {
				ip_addr_df = argv[++i];
			} else {
				std::cerr << "[MAIN] Error: no specified ip address" << std::endl;
				exit(EXIT_FAILURE);
			}
		} else if(strarg == "-p") {
			if (i < (argc - 1)) {
				port = atoi(argv[++i]);
			} else {
				std::cerr << "[MAIN] Error: no specified port" << std::endl;
				exit(EXIT_FAILURE);
			}

		} else if(strarg == "-d") {
			if (i < (argc - 1)) {
				std::string dir(argv[++i]);
				dataDirectory = dir;
			} else {
				std::cerr << "[MAIN] Erreur: no specified data directory" << std::endl;
				exit(EXIT_FAILURE);
			}
		} else if(strarg == "-l") {
			++i;
			while (i < argc) {
				std::string noeud(argv[i]);
				nodeList.push_back(noeud);
				i++;
			}
		}
		else if(strarg == "-b") {
			if (i < (argc - 1)) {
				batterySize = atoi(argv[++i]);
			} else {
				std::cerr << "[MAIN] Error: no specified battery size" << std::endl;
				exit(EXIT_FAILURE);
			}
		}
	}

	// data
	std::cout << "Adding data to list" << std::endl;

	DIR           *dir = opendir(dataDirectory.c_str());
	struct dirent *pdir;
	while ((pdir = readdir(dir))) {
		if(std::strlen(pdir->d_name) < 5) { continue; }
		dataList.push_back(pdir->d_name);
		dirData++;
		std::cout << "  Added " << pdir->d_name << " to data list." << std::endl;
	}

	// update dirData value
	dirData %= DATA_NOTIFIER;

	// threads
	pthread_t client,
			  listen;

	if (pthread_create(&client, NULL, clientFunc, NULL) != 0) {
		std::cerr << "[MAIN] Error while creating a new thread: client" << std::endl;
		pthread_exit(NULL);
	}

	if (pthread_create(&listen, NULL, listenServer, NULL) != 0) {
	std::cerr << "[MAIN] Error while creating a new thread: server" << std::endl;
	pthread_exit(NULL);
	}

	while(true) { } // Don't stop, we need to let thread run
	
	return EXIT_SUCCESS;
}
