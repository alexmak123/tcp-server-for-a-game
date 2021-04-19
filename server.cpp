
// СЕРВЕР ДЛЯ ИГРЫ

#include <cstdint>
#include <functional>
#include <thread>
#include <list>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <vector>
#include <utility>

#define PORT "3490"  // порт на сервере, к которому будут подключаться пользователи. Порт 3490, т.к. это стандартный порт для многопользовательских игр

#define BACKLOG 10	 // сколько ожидающих соединений будут храниться в очереди

#define MAXDATASIZE 100 // размер буффера для приёма данных от клиента в байтах

class Tcp_Server {
public :
     Tcp_Server ();
    ~Tcp_Server();
    void start_Server();
	void close_Server();
    void restart_Server();
    void listen_Server();
    void create_connection_for_two_players_Server();
private :
    int sockfd; // сокет для listen
    int new_fd[2]; // сокет для  accept
    int yes; // для функции setsockopt, которая позволит повторно использовать порт при перезапуске сервера. Это необходимо,тк порт при перезапуске будет занят прошлым сокетом
    std::vector <std::pair <sockaddr_storage , sockaddr_storage >> pairs_of_players;
};

// получаем тип адреса сокета, IPv4 или IPv6:
void *get_in_addr(struct sockaddr *sa) {

	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// что будет передавать наш сервер
char *modified_by_server_to_send (char *input) {
    char *res = (char*)malloc(strlen(input) + 4);
    size_t i = 0;
    for (i; i < strlen(input); i++) {
        res[i] = input[i];
    }
    res[i] = '!'; res[i+1] = '!'; res[i+2]='!'; res[i+3] = '\0';
    return res;
}

Tcp_Server::Tcp_Server() {
    sockfd = new_fd [0] = new_fd [1] = -1;
    yes = 1;
}

Tcp_Server::~Tcp_Server() {
    close_Server();
}

void Tcp_Server::start_Server() {
    struct addrinfo hints, *servinfo, *p; // нужно для функции getaddrinfo(), которая позволит заполнить инфу по серверу в servinfo
    int rv;

    // заполняем hints (начальные данные)
	memset(&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;  // можно любой тип : Ipv4 или Ipv6
	hints.ai_socktype = SOCK_STREAM; // сокет - потоковый
	hints.ai_flags = AI_PASSIVE; // для адреса сервера использовать мое IP

	// заполняем всю инфу про наш сервер в list servinfo и сразу проверяем на ошибки
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// наш сервер может иметь разные представления в рависимости от типа Ip, поэтому идём по всем возможным
	for(p = servinfo; p != NULL; p = p->ai_next) {
        // открываем сокет для listen и сразу делаем проверку
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

        // функция setsockopt позволит повторно использовать порт при перезапуске сервера. Это необходимо,тк порт при перезапуске будет занят прошлым сокетом. Сразу делаем проверку
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

        // биндим сокет на порт сервера (чтобы все соединения поступали на определенный порт сервера : 3490)
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(2);
	}

    freeaddrinfo(servinfo);
}

// тут недоработано, т.к надо подчищать зомби-процессы, вызванные fork
void Tcp_Server :: close_Server() {
    close(sockfd);
    close(new_fd[0]);
    close(new_fd[1]);
}

void Tcp_Server :: restart_Server() {
    close_Server();
    start_Server();
}

void Tcp_Server::listen_Server() {

    if (sockfd == 0) {
        perror("server: not initialized soket for listening\n");
        exit(1);
    }
    // "слушаем" входящие соединения на сокете для слушанья, пока их не примут с помощью accept
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("server: waiting for connections...\n");
}

void Tcp_Server::create_connection_for_two_players_Server() {

    struct sockaddr_storage addr1, addr2; // инфа про подключающихся
    socklen_t sin_size = sizeof (addr1); // accept() не поместит в addr обьём данных больше, чем в этой переменной
    char s[INET6_ADDRSTRLEN]; // максимальный размер адреса

    // новый сокет для accept()
    new_fd[0] = accept(sockfd, (struct sockaddr *)&addr1, &sin_size);
    if (new_fd[0] == -1) {
        perror("server : first accept");
    }

    // записываем сетевой адрес подключающегося в массив символов char s[max_size]
    inet_ntop(addr1.ss_family, get_in_addr((struct sockaddr *)&addr1), s, sizeof s);
    printf("server: got connection from %s\n", s);

    printf("server: waiting for second connection \n");

    // второе подключающийся
    new_fd[1] = accept(sockfd, (struct sockaddr *)&addr2, &sin_size);
    if (new_fd[1] == -1) {
        perror("server : second accept");
    }

    // записываем сетевой адрес подключающегося в массив символов char s[max_size]
    inet_ntop(addr2.ss_family, get_in_addr((struct sockaddr *)&addr2), s, sizeof s);
    printf("server: got second connection from %s\n", s);

    pairs_of_players.push_back(std::make_pair(addr1,addr2));

    if (fork() == 0) {
        close(sockfd);

        // пока мы не напишем exit, мы будем чатиться/играть
        char to_send[MAXDATASIZE];
        int step_number = 0;
        while (to_send != "!exit") {
            step_number++;
            to_send[MAXDATASIZE] = {0};
            // чей ход, у того есть право на rw, у второго есть право только на r. new_fd[1-step_number%2] - право на rw
            send(new_fd[1-step_number%2], "you have rights to read and write\n", MAXDATASIZE, 0);
            send(new_fd[step_number%2], "you have rights only to read\n", MAXDATASIZE, 0);
            // пока первый не скажет, что он закончил(finished), он может писать. Если он скажет exit, то чат закрываем
            while (to_send != "!finished" && to_send != "!exit") {
                // сначала читаем данные из буфера, которые передал пользователь с правом rw
                if (recv(new_fd[1-step_number%2], to_send, MAXDATASIZE-1, 0) == -1) {
                    perror("server recv");
                }

                // теперь передаем их пользователю, который имеет только право на чтение
                if (send(new_fd[step_number%2], to_send, MAXDATASIZE, 0) == -1) {
                    perror("server send");
                }
            }
        }

        close(new_fd[0]);
        close(new_fd[1]);
    }
    close(new_fd[0]);
    close(new_fd[1]);
}

using namespace std;

int main()
{
    Tcp_Server my_Server;
    my_Server.start_Server();
    my_Server.listen_Server();
    my_Server.create_connection_for_two_players_Server();
    return 0;
}
