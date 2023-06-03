#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BREAK_CONNECTION -101
#define MAX_COUNT_USERS 32
#define MAX_SIZE_HISTORY 20
#define EMPTY_MESSAGE -102
//#define ON_DEBUG
int flag_continue = 1;

struct user
{
	struct sockaddr_in addr;
	int history[20];
	int size_history;
	clock_t lastMessageTime = 0;
};

struct user db[MAX_COUNT_USERS];

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

void init_db()
{
	memset(db, 0, sizeof(db));
	for (int i = 0; i < MAX_COUNT_USERS; i++)
		for (int j = 0; j < MAX_SIZE_HISTORY; j++)
			db[i].history[j] = -1;
}

int findId(struct sockaddr_in* addr)
{
	int i;
	for (i = 0; i < MAX_COUNT_USERS; i++)
		if (!memcmp(&(db[i].addr), addr, sizeof(sockaddr_in)))
			return i;
	i = 0;
	while (!(db[i].addr.sin_addr.s_addr == 0 && db[i].addr.sin_port == 0))
		i++;
	db[i].addr.sin_addr.s_addr = addr->sin_addr.s_addr;
	db[i].addr.sin_port = addr->sin_port;
	db[i].addr.sin_family = addr->sin_family;
	//memcpy(&db[i].addr, addr, sizeof(sockaddr_in));
	//printf("new user with id #%d connected\n", i);
	return i;
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

int already_recieved(int index, int num)
{
	for (int i = 1; i < MAX_SIZE_HISTORY; i++)
		if (db[index].history[i] == num)
			return 1;
	return 0;
}

void add_to_history(int index, int num)
{
	for (int i = MAX_SIZE_HISTORY - 1; i > 0; i--)
		db[index].history[i] = db[index].history[i - 1];
	db[index].history[0] = num;
	if (db[index].size_history < 20)
		db[index].size_history++;
}

void updateTime(int index)
{
	db[index].lastMessageTime = clock();
}

void printToFile(int num, struct sockaddr_in addr, char* buf, unsigned short AA, int BBB, char* hh, char* mm, char* ss, int id)
{
	FILE* f = fopen("msg.txt", "a+");
	fprintf(f, "%d.%d.%d.%d:%d %hu %d %s:%s:%s ", (ntohl(addr.sin_addr.s_addr) >> 24) & 0xff, (ntohl(addr.sin_addr.s_addr) >> 16) & 0xff, (ntohl(addr.sin_addr.s_addr) >> 8) & 0xff, ntohl(addr.sin_addr.s_addr) & 0xff, ntohs(addr.sin_port), AA, BBB, hh, mm, ss);
	for (int i = 0; i < num; i++)
		fprintf(f, "%c", buf[17 + i]);
	//printf("received message: %s from user #%d\n", &buf[17], id);
	fprintf(f, "\n");
	fclose(f);
}

int receive_mes(int cs, int index, sockaddr_in addr, int* curId)
{
	
	char buf[5000];
	char tmp[4], tmp1[2];
	memset(buf, 0, sizeof(buf));
	int addrLen = sizeof(addr), flags = 0;
	int sizeRecv = recvfrom(cs, buf, 5000, flags, (sockaddr*)&addr, &addrLen);//получение сообщения
	int id = findId(&addr);//ищем в базе ID текущего клиента либо добавляем нового
	*(curId) = id;
	int i = 0;
	tmp[0] = buf[0];
	tmp[1] = buf[1];
	tmp[2] = buf[2];
	tmp[3] = buf[3];
	int n = ntohl(*(int*)tmp);
	
	for (int i = 0; i < MAX_SIZE_HISTORY; i++)
		if (db[id].history[i] == htonl(n))
			return 0;//если это сообщение уже было получено, прекратить его обработку и запись

	tmp1[0] = buf[4];
	tmp1[1] = buf[5];
	unsigned short AA = ntohs(*(int*)tmp1);
	
	tmp[0] = buf[6];
	tmp[1] = buf[7];
	tmp[2] = buf[8];
	tmp[3] = buf[9];
	int BBB = ntohl(*(int*)tmp);

	char hh[3];
	hh[0] = buf[10];
	int h = hh[0];
	hh[0] = h / 10 + '0';
	hh[1] = h % 10 + '0';
	hh[2] = '\0';
	char mm[3];
	mm[0] = buf[11];
	int m = mm[0];
	mm[0] = m / 10 + '0';
	mm[1] = m % 10 + '0';
	mm[2] = '\0';
	char ss[3];
	ss[0] = buf[12];
	int s = ss[0];
	ss[0] = s / 10 + '0';
	ss[1] = s % 10 + '0';
	ss[2] = '\0';

	tmp[0] = buf[13];
	tmp[1] = buf[14];
	tmp[2] = buf[15];
	tmp[3] = buf[16];
	
	int num = ntohl(*(int*)tmp);
	
	if (num > 0)
	{
		printToFile(num, addr, buf, AA, BBB, hh, mm, ss, id);
		if (!already_recieved(id, ntohl(n)))
		{
			add_to_history(id, ntohl(n));
			updateTime(id);
		}
		if (num == 4 && buf[17] == 's' && buf[18] == 't' && buf[19] == 'o' && buf[20] == 'p' && buf[21] == '\0')
			flag_continue = 0;
		return sizeRecv;
	}
	else//пустое сообщение
		return EMPTY_MESSAGE;
}

void send_resp(int s, int id)
{
	int flags = 0;
	sockaddr_in* addr = (sockaddr_in*)malloc(sizeof(sockaddr_in));
	memset(addr, 0, sizeof(addr));
	int addrlen = sizeof(addr);
	memcpy(addr, &db[id].addr, sizeof(db[id].addr));

	char* sendBuf = (char*)db[id].history;
	int ret = sendto(s, sendBuf, db[id].size_history * 4, flags, (sockaddr*)addr, 16);
	if (ret < 0)
		sock_err("sendto", s);
	free(addr);
}

int main(int argc, char* argv[])
{
#ifdef ON_DEBUG
	int port_max = 9005, port_min = 9000;
#else
	int port_max = str_to_int(argv[2]), port_min = str_to_int(argv[1]);
#endif
	int range = port_max - port_min + 1;
	int flags = 0;
	init_db();

	init();

	struct sockaddr_in* addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in) * range);
	memset(addr, 0, sizeof(addr));

	for (int i = 0; i < range; i++)
	{
		addr[i].sin_family = AF_INET;
		addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
		addr[i].sin_port = htons(port_min + i);
	}

	int* sockets = (int*)malloc(sizeof(int) * range);
	for (int i = 0; i < range; i++)
	{
		sockets[i] = socket(AF_INET, SOCK_DGRAM, 0);
		set_non_block_mode(sockets[i]);
	}

	for (int i = 0; i < range; i++)
	{
		if (bind(sockets[i], (struct sockaddr*)&addr[i], sizeof(addr[i])) < 0)
			return sock_err("bind", sockets[i]);
	}

	printf("Listening on: ");
	for (int i = 0; i < range - 1; i++)
		printf("%d, ", port_min + i);
	printf("%d\n", port_max);

	struct pollfd* pfd = (struct pollfd*)malloc(sizeof(struct pollfd) * range);
	for (int i = 0; i < range; i++)
	{
		pfd[i].fd = sockets[i];
		pfd[i].events = POLLIN | POLLOUT;
	}

	char buf[255];
	memset(buf, 0, sizeof(buf));

	while (flag_continue)
	{
		// Ожидание событий в течение 1 сек
		int ev_cnt = WSAPoll(pfd, range, 1000);

		if (ev_cnt > 0)
		{
			for (int i = 0; i < range; i++)
			{
				if (pfd[i].revents & POLLIN)
				{
					int id = 0;
					int* id_ptr = &id;
					int ret = receive_mes(pfd[i].fd, i, addr[i], id_ptr);
					if (ret != EMPTY_MESSAGE)
						send_resp(pfd[i].fd, *id_ptr);
				}
			}
		}
		
		for (int i = 0; i < MAX_COUNT_USERS; i++)
		{
			clock_t dif = int((clock() - db[i].lastMessageTime) / CLOCKS_PER_SEC);
			if (dif >= 30 && db[i].addr.sin_addr.s_addr != 0)
			{
				//printf("user #%d was deleted from db\n", i);
				memset(&db[i], 0, sizeof(db[i]));
				memset(db[i].history, -1, sizeof(db[i].history));
			}
		}
	}
	free(pfd);
	free(sockets);
	free(addr);
	//printf("\'stop\' message arrived. Terminating...\n");
}