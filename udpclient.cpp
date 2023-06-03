#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BREAK_CONNECTION -2
#define STOP_MESSAGE -3
#define MAX_ID 100
#define MAX_HISTORY 20
#define MAX_SIZE_MESSAGE 25000
#define MAX_MESSAGES 1000
//#define ON_DEBUG
char message[MAX_MESSAGES][MAX_SIZE_MESSAGE];
int count = 0;
int sent = 0;
int array_sent[256];

int read_file(char* name)
{
	FILE* f;
	f = fopen(name, "r+");
	int num_str = 0, i = 0, count = 0;
	char c;
	if (f != 0)
	{
		while (feof(f) == 0)
		{
			fgets(message[i], MAX_SIZE_MESSAGE, f);
			if (message[i][0] == '\n')
				continue;
			i++;
		}
		fclose(f);
	}
	else
		printf("File open error\n");
	for (int j = 0; j <= i; j++)
		if (strlen(message[j]) > 0 && message[j][strlen(message[j]) - 1] == '\n')
			message[j][strlen(message[j]) - 1] = '\0';
	i = 0;
	while (message[i][0] != '\0')
	{
		count++;
		i++;
	}
	return count;
}


int init()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSAStartup перед началом использования сокетов
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
	return 1; // Для других ОС действий не требуется
#endif
}

void deinit()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSACleanup в конце работы
	WSACleanup();
#else
	// Для других ОС действий не требуется
#endif
}

int sock_err(const char* function, int s)
{
	int err;
#ifdef _WIN32
	err = WSAGetLastError();
#else
	err = errno;
#endif
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

void s_close(int s)
{
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
}

int set_non_block_mode(int s)
{
#ifdef _WIN32
	unsigned long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int fl = fcntl(s, F_GETFL, 0);
	return fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
}

int str_to_int(char* str)
{
	int i = 0, res = 0;
	while (str[i] != '\0')
	{
		res = res * 10 + str[i] - '0';
		i++;
	}
	return res;
}

void strncpy_my(char* dst, char* src, int count) 
{
	while (count)
	{ 
		count--;
		dst[count] = src[count];
	}
}

int read_addr(char* str)
{
	int i = 0, port = 0;
	while (str[i] != ':')
		i++;
	str[i++] = '\0';
	while (str[i] != '\0')
	{
		port = port * 10 + (str[i] - '0');
		i++;
	}
	return port;
}

void send_info(int s, int i, struct sockaddr_in addr)
{
	char result[5000];
	memset(result, 0, sizeof(result));
	int flags = 0, addrlen = sizeof(addr);

	int numMessage = htonl(i);
	strncpy_my(result, (char*)&numMessage, 4);//номер сообщения
	
	char* tmpMes = (char*)malloc(sizeof(char) * strlen(message[i]));
	strcpy(tmpMes, message[i]);
	char* ptr = strtok(tmpMes, " ");

	int tmp = htons(atoi(ptr));
	strncpy_my(result + 4, (char*)&tmp, 2);//AA

	tmp = htonl(atoi(strtok(NULL, " ")));
	strncpy_my(result + 6, (char*)&tmp, 4);//BBB

	ptr = strtok(NULL, ":");
	unsigned char out = str_to_int(ptr);
	result[10] = out;//hh

	ptr = strtok(NULL, ":");
	out = str_to_int(ptr);
	result[11] = out;//mm

	ptr = strtok(NULL, " ");
	out = str_to_int(ptr);
	result[12] = out;//ss

	ptr = strtok(NULL, "\0");
	tmp = strlen(ptr);
	tmp = htonl(tmp);
	strncpy_my(result + 13, (char*)&tmp, 4);//длина сообщения
	strncpy_my(result + 17, ptr, strlen(ptr));//сообщение
	
	int ret = sendto(s, result, 17 + strlen(ptr), flags, (sockaddr*)&addr, addrlen);
	if (ret < 0)
		sock_err("sendto", s);
}

void recv_info(int s)
{
	struct timeval tv = { 0, 100 * 1000 };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(s, &fds);
	int result = select(s + 1, &fds, 0, 0, &tv);

	if (result > 0)
	{
		int datagram[20];
		memset(datagram, 0, 20 * sizeof(int));
		struct sockaddr_in addr;
		socklen_t  addrlen = sizeof(addr);
		int received = recvfrom(s, (char*)datagram, sizeof(datagram), 0, (struct sockaddr*)&addr, &addrlen);
		if (received < 0)
			sock_err("recvfrom", s);
		else
		{
			printf("received datagram is:");
			for (int i = 0; i < received / 4; i++)
			{
				printf("%d ", ntohl(datagram[i]));
				if (ntohl(datagram[i]) >= 0 && array_sent[ntohl(datagram[i])] == 0)
				{
					array_sent[ntohl(datagram[i])] = 1;
					sent++;
				}
			}
			printf("\n");
			
		}
	}
	return;
}

int main(int argc, char* argv[])
{
#ifdef ON_DEBUG
	char ip[25] = "192.168.43.97";
	int port = 9002;
	count = read_file((char*)"cli2.txt");
#else // ON_DEBUG
	char ip[25];
	memset(ip, 0, sizeof(ip));
	strcpy(ip, argv[1]);
	int port = read_addr(ip);
	count = read_file(argv[2]);
#endif

	init();

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	int ret = 0;
	int s;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	int addrlen = sizeof(addr), flags = 0;

	for (int i = 0; (count >= 20 && sent < 20) || (count < 20 && sent < count); i = (i + 1) % count)
	{
		if (array_sent[i] == 0)
		{
			send_info(s, i, addr);
			printf("try to send message #%d\n", i);
			recv_info(s);
		}
	}
	printf("%d messages sent. Terminating...\n", sent);
	deinit();
	return 0;
}