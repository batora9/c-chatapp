#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <pthread.h>

#define BUFSIZE 256

// グローバル変数として現在の名前を保持
char current_name[64] = "";

void chop(char *str)
{
    char *p = strchr(str, '\n');
    if (p != NULL)
        *p = '\0';
}

void *receive_thread(void *arg)
{
    int socket_fd = *(int *)arg;
    char buffer[BUFSIZE];

    while (1)
    {
        memset(buffer, '\0', BUFSIZE);
        int bytes_received = recv(socket_fd, buffer, BUFSIZE, 0);
        if (bytes_received <= 0)
        {
            printf("\nサーバとの接続が切断されました\n");
            break;
        }
        if (strcmp(buffer, "quit") == 0)
        {
            break;
        }

        // サーバーからの初期名前設定メッセージをチェック
        if (strncmp(buffer, "INITIAL_NAME:", 13) == 0)
        {
            strcpy(current_name, buffer + 13);
            printf("\033[90mあなたの名前は '%s' です。\033[0m\n", current_name);
            continue;
        }

        // 通常のメッセージを表示
        printf("\r%s\n[%s]> ", buffer, current_name);
        fflush(stdout);
    }

    return NULL;
}

void *send_thread(void *arg)
{
    int socket_fd = *(int *)arg;
    char buffer[BUFSIZE];

    // 少し待機してサーバーからの初期名前を受信
    sleep(1);

    printf("\033[90m利用可能なコマンドを表示するには ':help' を入力してください。\033[0m\n");
    printf("[%s]> ", current_name);
    fflush(stdout);

    while (1)
    {
        memset(buffer, '\0', BUFSIZE);
        if (fgets(buffer, BUFSIZE, stdin) == NULL)
        {
            strcpy(buffer, "quit");
        }
        chop(buffer);

        // 空のメッセージは送信しない
        if (strlen(buffer) == 0)
        {
            printf("[%s]> ", current_name);
            fflush(stdout);
            continue;
        }

        send(socket_fd, buffer, strlen(buffer) + 1, 0);
        if (strcmp(buffer, "quit") == 0)
        {
            break;
        }

        // 名前変更コマンドの場合は少し待機してから次のプロンプトを表示
        if (strncmp(buffer, ":name ", 6) == 0)
        {
            strcpy(current_name, buffer + 6);
            usleep(100000); // 100ms待機してサーバーからの応答を待つ
            continue;
        }

        printf("[%s]> ", current_name);
        fflush(stdout);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int socket_fd;
    struct sockaddr_in server;
    struct hostent *hp;
    uint16_t port;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[2]);

    // ソケットの作成: INET ドメイン・ストリーム型
    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("client: socket");
        exit(EXIT_FAILURE);
    }

    // サーバプロセスのソケットアドレス情報の設定
    memset((void *)&server, 0, sizeof(server)); // アドレス情報構造体の初期化
    server.sin_family = PF_INET;                // プロトコルファミリの設定
    server.sin_port = htons(port);              // ポート番号の設定

    // argv[1] のマシンの IP アドレスを返す
    if ((hp = gethostbyname(argv[1])) == NULL)
    {
        perror("client: gethostbyname");
        exit(EXIT_FAILURE);
    }
    // IP アドレスの設定
    memcpy(&server.sin_addr, hp->h_addr_list[0], hp->h_length);

    // サーバに接続
    if (connect(socket_fd, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("client: connect");
        exit(EXIT_FAILURE);
    }
    printf("\033[90mサーバに接続されました\033[0m\n");

    pthread_t recv_tid, send_tid;

    // スレッドの作成
    if (pthread_create(&recv_tid, NULL, receive_thread, &socket_fd) != 0)
    {
        perror("Thread creation failed (receive)");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&send_tid, NULL, send_thread, &socket_fd) != 0)
    {
        perror("Thread creation failed (send)");
        exit(EXIT_FAILURE);
    }

    // quit を送信したときにスレッドが終了しているか判定
    while (1)
    {
        if (pthread_tryjoin_np(send_tid, NULL) == 0)
        {
            printf("\n\033[90mチャットを終了しました\033[0m\n");
            break;
        }
    }

    close(socket_fd);

    return 0;
}
