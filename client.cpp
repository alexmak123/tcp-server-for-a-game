
// КЛИЕНТ ДЛЯ СЕРВЕРА

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "3490" // порт, к которому будет подключаться клиент

#define MAXDATASIZE 100 // максимальное количество байт, которое мы можем получить за раз от сервера

#define IP "84.201.189.157" // клиент коннектится к хосту с глобальным IP 84.201.189.157

// получаем тип адреса сокета, IPv4 или IPv6:
void *get_in_addr(struct sockaddr *sa) {

	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

class Client {
public :
    Client();
    ~Client();
    void connect_Client();
    void read_and_write_Client();
private :
    int sockfd; // сокет для подключения
    int rights; // 0 - только для чтения, 1 - для записи и чтения
};

Client::Client() {

}

Client::~Client() {
    close(sockfd);
}

void Client :: connect_Client() {
    char s[INET6_ADDRSTRLEN]; // максимальный размер адреса сервера
    struct addrinfo hints, *servinfo, *p; // нужно для функции getaddrinfo(), которая позволит заполнить инфу по клиенту в servinfo
    int rv;

    // заполняем hints (грубо говоря начальные данные)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // можно любой тип : Ipv4 или Ipv6
	hints.ai_socktype = SOCK_STREAM; // сокет - потоковый

    // заполняем всю инфу про наш клиент в list servinfo и сразу проверяем на ошибки. Указываем хост IP
	if ((rv = getaddrinfo(IP, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// цикл по всем результатам и подключение к первому подходящему
	for(p = servinfo; p != NULL; p = p->ai_next) {
        // открываем сокет для подключения и сразу делаем проверку
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

        // делаем сразу connect. Нам не нужно ничего биндить, тк нам не важно с какого порта клиент подключается, нам важно знать только порт сервера.
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		exit(2);
	}

	// записываем сетевой адрес к чему подключаемся в массив символов char s[max_size]
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo);
}

void Client::read_and_write_Client() {
    // мы уже подключены к серверу, поэтому можем считать, что он нам прислал
    char buf[MAXDATASIZE];
    if (recv(sockfd, buf, MAXDATASIZE-1, 0) == -1) {
	    perror("client recv");
	    exit(1);
	}
	rights = 0;
    if (buf == "you have rights to read and write\n") {
        rights = 1;
    }

    while (1) {
        int amount_of_messages = 0;
        if (rights == 1) {
            while (amount_of_messages < 11) {
                amount_of_messages++;
                fgets(buf, MAXDATASIZE, stdin);
                if (send(sockfd, buf, MAXDATASIZE, 0) == -1) {
                    perror("client send");
                }
            }
        } else {
            while (amount_of_messages < 11) {
                amount_of_messages++;
                sleep(10);
                if (recv(sockfd, buf, MAXDATASIZE-1, 0) == -1) {
                    perror("client recv");
                }
                printf("another user sended:  %s\n",buf);
            }
        }
        rights = (rights + 1) % 2;
    }

}

int main() {

    Client A;
    A.connect_Client();
    A.read_and_write_Client();
	return 0;
}
