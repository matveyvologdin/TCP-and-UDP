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

#define N 100
#define BREAK_CONNECTION -2
#define EMPTY_MESSAGE -3
#define INVALID_START_MESSAGE -100
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

int sock_err(const char *function, int s)
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

int recv_string(int cs)
{
    char buffer[512];
    int curlen = 0;
    int rcv;
    do
    {
        int i;
        rcv = recv(cs, buffer, sizeof(buffer), 0);
        for (i = 0; i < rcv; i++)
        {
            if (buffer[i] == '\n')
                return curlen;
            curlen++;
        }
        if (curlen > 5000)
        {
            printf("input string too large\n");
            return 5000;
        }
    } while (rcv > 0);
    return curlen;
}

int send_notice(int cs, int len)
{
    char buffer[1024];
    int sent = 0;
    int ret;
#ifdef _WIN32
    int flags = 0;
#else
    int flags = MSG_NOSIGNAL;
#endif
    sprintf(buffer, "Length of your string: %d chars.", len);
    while (sent < (int)strlen(buffer))
    {
        ret = send(cs, buffer + sent, strlen(buffer) - sent, flags);
        if (ret <= 0)
            return sock_err("send", cs);
        sent += ret;
    }
    return 0;
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

int receive_mes(int cs, int ip, int port)
{
    FILE *f = fopen("msg.txt", "a+");
    char buf[25500];
    memset(buf, 0, sizeof(buf));

    recv(cs, buf, 4, 0); //чтение номера сообщения
    int n = ntohl(*(int *)buf);
    memset(buf, 0, sizeof(buf));

    recv(cs, buf, 2, 0); //чтение AA
    unsigned short AA = ntohs(*(int *)buf);
    // fprintf(f, "%d ", n1);
    memset(buf, 0, sizeof(buf));

    recv(cs, buf, 4, 0); //чтение BBB
    int BBB = ntohl(*(int *)buf);
    // fprintf(f, "%d ", n);
    memset(buf, 0, sizeof(buf));

    recv(cs, buf, 1, 0); //чтение hh
    int h = *(int *)buf;
    char hh[3];
    hh[0] = h / 10 + '0';
    hh[1] = h % 10 + '0';
    hh[2] = '\0';
    memset(buf, 0, sizeof(buf));

    recv(cs, buf, 1, 0); //чтение mm
    int m = *(int *)buf;
    char mm[3];
    mm[0] = m / 10 + '0';
    mm[1] = m % 10 + '0';
    mm[2] = '\0';
    memset(buf, 0, sizeof(buf));

    recv(cs, buf, 1, 0); //чтение ss
    int s = *(int *)buf;
    char ss[3];
    ss[0] = s / 10 + '0';
    ss[1] = s % 10 + '0';
    ss[2] = '\0';
    memset(buf, 0, sizeof(buf));

    recv(cs, buf, 4, 0); //чтение N
    int num = ntohl(*(int *)buf);
    memset(buf, 0, sizeof(buf));

    if (num > 0)
    {
        recv(cs, buf, num, 0); //чтение message
        fprintf(f, "%d.%d.%d.%d:%d %d %d %s:%s:%s %s\n", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, port, AA, BBB, hh, mm, ss, buf);
    }
    else
    {
        fclose(f);
        return EMPTY_MESSAGE;
    }
    if (strcmp(buf, "stop") == 0)
    {
        fclose(f);
        return BREAK_CONNECTION;
    }
    fclose(f);
    return 1;
}

int main(int argc, char *argv[])
{
    int port = atoi(argv[1]);
    int ls;
    struct sockaddr_in addr;
    // Инициалиазация сетевой библиотеки
    init();
    // Создание TCP-сокета
    ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0)
        return sock_err("socket", ls);
    set_non_block_mode(ls);
    // Заполнение адреса прослушивания
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);              // Сервер прослушивает порт 9000
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Все адреса
                                              // Связывание сокета и адреса прослушивания
    int addrlen = sizeof(addr);
    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return sock_err("bind", ls);
    // Начало прослушивания
    if (listen(ls, 64) < 0)
        return sock_err("listen", ls);

    int ip;
    int cs[N]; // Сокеты с подключенными клиентами
    memset(cs, 0, sizeof(cs));
    char buf[3];
    int cur_count = 0;
    fd_set rfd;
    fd_set wfd;
    int nfds = ls;
    int i, ret = 0;
    struct timeval tv = {1, 0};

    while (1)
    {
        FD_ZERO(&rfd);
        FD_ZERO(&wfd);
        FD_SET(ls, &rfd);
        for (i = 0; i < N; i++)
        {
            FD_SET(cs[i], &rfd);
            FD_SET(cs[i], &wfd);
            if (nfds < cs[i])
                nfds = cs[i];
        }
        if (select(nfds + 1, &rfd, &wfd, 0, &tv) > 0)
        {
            // Есть события
            if (FD_ISSET(ls, &rfd))
            {
                // Есть события на прослушивающем сокете, можно вызвать accept,принять
                // подключение и добавить сокет подключившегося клиента в массив cs

                cs[cur_count] = accept(ls, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
                sleep(1);
                set_non_block_mode(cs[cur_count]);
                recv(cs[cur_count], buf, 3, 0);
                if (strcmp(buf, "put"))
                    return INVALID_START_MESSAGE;
                cur_count++;
            }
            for (i = 0; i < N; i++)
            {
                if (FD_ISSET(cs[i], &rfd))
                {
                    // Сокет cs[i] доступен для чтения. Функция recv вернет данные, recvfrom - дейтаграмму
                    ip = ntohl(addr.sin_addr.s_addr);
                    ret = receive_mes(cs[i], ip, port);
                    //sleep(1);
                    if (ret != EMPTY_MESSAGE)
                        send(cs[i], "ok", 2, 0);
                    //sleep(1);
                    if (ret == BREAK_CONNECTION)
                    {
                        for (i = 0; i < N; i++)
                            if (cs[i] != 0)
                                s_close(cs[i]);
                        s_close(ls);
                        deinit();
                        return 0;
                    }
                }
            }
        }
        else
        {
            sock_err("select", ls);
            // Произошел таймаут или ошибка
        }
    }
    for (i = 0; i < N; i++)
        if (cs[i] != 0)
            s_close(cs[i]);
    s_close(ls);
    deinit();
    return 0;
}