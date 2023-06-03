#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#pragma comment(lib, "ws2_32.lib")

//#define ON_DEBUG
#define MAX_SIZE_MESSAGE 25000
#define MAX_MESSAGES 100
#define EMPTY_LINE -2
#define STOP_MESSAGE -3
char message[MAX_MESSAGES][MAX_SIZE_MESSAGE];
int flagGet = 0;
int init();
void deinit();
int sock_err(const char*,int);
void s_close(int);
int str_to_int(char*);
int send_info(int, int);
int read_file(char*);
int read_addr(char*);
int recv_ok(int);
int receive_mes(int, int, int, char*);
int main(int argc, char* argv[])
{
	int count = 0, count_end = 0;
#ifdef ON_DEBUG
	char ip[25] = "127.0.0.1";
	flagGet = 0;
	int port = 9000;
	char nameFile[100];
	memset(nameFile, 0, sizeof(nameFile));
	scanf("%s", nameFile);
	count = read_file(nameFile);
#else // ON_DEBUG
	char ip[25];
	memset(ip, 0, sizeof(ip));
	strcpy(ip, argv[1]);
	int port = read_addr(ip);
	if (!strcmp(argv[2], "get"))
		flagGet = 1;
	else
		count = read_file(argv[2]);
	
#endif

	int s;
	struct sockaddr_in addr;

	init();

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket", s);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);
	int i;
	for (i = 0; i < 10; i++)
	{
		if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR && i < 9)
		{
			Sleep(100);
			printf("Cannot connect to server(%d)\n", i + 1);
		}
		else if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR && i == 9)
		{
			printf("Connection timeout\n");
			return 0;
		}
		else
		{
			//Соединение установлено
			printf("Connected\n");
			int flags = 0;
			int ret;
			if (flagGet == 0)//режим отправки сообщений
				ret = send(s, "put", 3, flags);
			else//режим получения содержимого
				ret = send(s, "get", 3, flags);
			
			if (ret < 0)
				return sock_err("send", s);
			break;
		}
	}
	if (flagGet == 0)
	{
		int ret;
		i = 0;
		count_end = count;
		while (i < count)
		{
			ret = send_info(s, i);
			if (ret == STOP_MESSAGE)
				count_end = i + 1;
			i++;
		}
		count = count_end;
		i = 0;
		while (i < count)
		{
			ret = recv_ok(s);
			i++;
			//printf("ok - received\n");
		}
	}
	else //flagGet == 1
	{
		int ret;
		int curIP = ntohl(inet_addr(ip));
		do {
			ret = receive_mes(s, curIP, port, (char*)"msg.txt");
		} while (ret > 0);
		//receive_mes(s, curIP, port, argv[3]);
	}
	s_close(s);
	deinit();

	return 0;
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

int init()
{
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}

void deinit()
{
	// Для Windows следует вызвать WSACleanup в конце работы
	WSACleanup();
}

int sock_err(const char* function, int s)
{
	int err;
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

void s_close(int s)
{
	closesocket(s);
}

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

int send_info(int s, int i)
{
	int flags = 0;
	int k = htonl(i);
	int ret = send(s, (char*)&k, 4, flags);
	char* ptr = strtok(message[i], " ");

	int tmp = atoi(ptr);
	tmp = htons(tmp);
	ret = send(s, (char*)&tmp, 2, flags);//Отправка АА
	if (ret < 0)
		return sock_err("send", s);

	ptr = strtok(NULL, " ");
	tmp = atoi(ptr);
	tmp = htonl(tmp);
	ret = send(s, (char*)&tmp, 4, flags);//Отправка BBB
	if (ret < 0)
		return sock_err("send", s);

	ptr = strtok(NULL, ":");
	char out = str_to_int(ptr);
	ret = send(s, (char*)&out, 1, flags);//Отправка hh
	if (ret < 0)
		return sock_err("send", s);

	ptr = strtok(NULL, ":");
	out = str_to_int(ptr);
	ret = send(s, (char*)&out, 1, flags);//Отправка mm
	if (ret < 0)
		return sock_err("send", s);

	ptr = strtok(NULL, " ");
	out = str_to_int(ptr);
	ret = send(s, (char*)&out, 1, flags);//Отправка ss
	if (ret < 0)
		return sock_err("send", s);

	ptr = strtok(NULL, "\0");
	tmp = strlen(ptr);
	tmp = htonl(tmp);
	ret = send(s, (char*)&tmp, 4, flags);//Отправка длины сообщения
	if (ret < 0)
		return sock_err("send", s);
	ret = send(s, ptr, strlen(ptr), flags);//Отправка сообщения
	if (ret < 0)
		return sock_err("send", s);
	if (strcmp(ptr, "stop") == 0)
		return STOP_MESSAGE;
	return 1;
}

int receive_mes(int cs, int ip, int port, char* file)
{
	FILE* f = fopen(file, "a+");
	char buf[5000];
	memset(buf, 0, sizeof(buf));
	
	int ret = recv(cs, buf, 4, 0); //чтение номера сообщения
	if (ret <= 0)
		return -1;
	int n = ntohl(*(int*)buf);
	memset(buf, 0, sizeof(buf));

	recv(cs, buf, 2, 0); //чтение AA
	if (ret <= 0)
		return -1;
	unsigned short AA = ntohs(*(int*)buf);
	memset(buf, 0, sizeof(buf));

	recv(cs, buf, 4, 0); //чтение BBB
	if (ret <= 0)
		return -1;
	int BBB = ntohl(*(int*)buf);
	memset(buf, 0, sizeof(buf));

	recv(cs, buf, 1, 0); //чтение hh
	if (ret <= 0)
		return -1;
	int h = *(int*)buf;
	char hh[3];
	hh[0] = h / 10 + '0';
	hh[1] = h % 10 + '0';
	hh[2] = '\0';
	memset(buf, 0, sizeof(buf));

	recv(cs, buf, 1, 0); //чтение mm
	if (ret <= 0)
		return -1;
	int m = *(int*)buf;
	char mm[3];
	mm[0] = m / 10 + '0';
	mm[1] = m % 10 + '0';
	mm[2] = '\0';
	memset(buf, 0, sizeof(buf));

	recv(cs, buf, 1, 0); //чтение ss
	if (ret <= 0)
		return -1;
	int s = *(int*)buf;
	char ss[3];
	ss[0] = s / 10 + '0';
	ss[1] = s % 10 + '0';
	ss[2] = '\0';
	memset(buf, 0, sizeof(buf));

	recv(cs, buf, 4, 0); //чтение N
	if (ret <= 0)
		return -1;
	int num = ntohl(*(int*)buf);
	memset(buf, 0, sizeof(buf));

	if (num > 0)
	{
		recv(cs, buf, num, 0); //чтение message
		if (ret <= 0)
			return -1;
		fprintf(f, "%d.%d.%d.%d:%d %d %d %s:%s:%s %s\n", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, port, AA, BBB, hh, mm, ss, buf);
	}
	fclose(f);
	return 17 + num;
}

int recv_ok(int s)
{
	char buf[2];
	int ret;
	do {
		ret = recv(s, buf, 2, 0);
	} while (ret <= 0);
	return 1;
}