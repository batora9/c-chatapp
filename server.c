#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "commands.h"

void chop(char *str)
{
    char *p = strchr(str, '\n');
    if (p != NULL)
        *p = '\0';
}

// 送信者以外の全クライアントにメッセージを送信
void broadcast_message(client_info_t clients[], int max_clients, int sender_index, const char *message)
{
    char broadcast_buf[BUFSIZE + 128];
    snprintf(broadcast_buf, sizeof(broadcast_buf), "[%s]: %s", clients[sender_index].client_name, message);

    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].socket_fd != UNUSED && i != sender_index)
        {
            send(clients[i].socket_fd, broadcast_buf, strlen(broadcast_buf) + 1, 0);
        }
    }
}

// クライアントを削除
void remove_client(client_info_t clients[], int max_clients, int index)
{
    if (index >= 0 && index < max_clients && clients[index].socket_fd != UNUSED)
    {
        char leaving_name[64];
        strcpy(leaving_name, clients[index].client_name);

        printf("クライアント %s が切断されました\n", leaving_name);
        close(clients[index].socket_fd);
        clients[index].socket_fd = UNUSED;
        memset(clients[index].client_name, 0, sizeof(clients[index].client_name));

        // 他のクライアントに切断を通知
        char leave_notification[256];
        snprintf(leave_notification, sizeof(leave_notification), "\033[35m*** %s がチャットから退出しました ***\033[0m", leaving_name);
        for (int i = 0; i < max_clients; i++)
        {
            if (clients[i].socket_fd != UNUSED)
            {
                send(clients[i].socket_fd, leave_notification, strlen(leave_notification) + 1, 0);
            }
        }
    }

    // クライアント数が0になったら終了
    int active_clients = 0;
    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].socket_fd != UNUSED)
        {
            active_clients++;
        }
    }
    if (!active_clients)
    {
        printf("すべてのクライアントが切断されました。サーバを終了します。\n");
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char *argv[])
{
    int listening_socket;
    int connected_socket;
    struct sockaddr_in server;
    struct sockaddr_in client;
    socklen_t fromlen;
    uint16_t port;
    char buffer[BUFSIZE];
    int temp = 1;
    fd_set readfds, masterfds;
    int max_fd;
    client_info_t clients[MAX_CLIENTS];

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);

    // クライアント配列の初期化
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].socket_fd = UNUSED;
    }

    // ソケットの作成
    listening_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (listening_socket == -1)
    {
        perror("server: socket");
        exit(EXIT_FAILURE);
    }

    // ソケットオプションの設定
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&temp, sizeof(temp)))
    {
        perror("server: setsockopt");
        exit(EXIT_FAILURE);
    }

    // サーバプロセスのソケットアドレス情報の設定
    memset((void *)&server, 0, sizeof(server)); // アドレス情報構造体の初期化
    server.sin_family = PF_INET;                // プロトコルファミリの設定
    server.sin_port = htons(port);              // ポート番号の設定
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // ソケットにアドレスをバインド
    if (bind(listening_socket, (struct sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("server: bind");
        exit(EXIT_FAILURE);
    }

    // 接続要求の受け入れ準備
    if (listen(listening_socket, 5) == -1)
    {
        perror("server: listen");
        exit(EXIT_FAILURE);
    }

    printf("クライアントの接続を待機中...\n");

    // ファイルディスクリプタセットの初期化
    FD_ZERO(&masterfds);
    FD_SET(listening_socket, &masterfds);
    max_fd = listening_socket;

    while (1)
    {
        readfds = masterfds;

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            continue;
        }

        // 新しい接続をチェック
        if (FD_ISSET(listening_socket, &readfds))
        {
            fromlen = sizeof(client);
            connected_socket = accept(listening_socket, (struct sockaddr *)&client, &fromlen);

            if (connected_socket != -1)
            {
                int client_index = -1;
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].socket_fd == UNUSED)
                    {
                        client_index = i;
                        break;
                    }
                }

                if (client_index != -1)
                {
                    clients[client_index].socket_fd = connected_socket;
                    clients[client_index].addr = client;
                    snprintf(clients[client_index].client_name, sizeof(clients[client_index].client_name), "Client%d", client_index + 1);

                    FD_SET(connected_socket, &masterfds);
                    if (connected_socket > max_fd)
                    {
                        max_fd = connected_socket;
                    }

                    printf("新しいクライアント %s が接続されました\n", clients[client_index].client_name);

                    // 初期名前をクライアントに送信
                    char name_msg[BUFSIZE];
                    snprintf(name_msg, sizeof(name_msg), "INITIAL_NAME:%s", clients[client_index].client_name);
                    send(connected_socket, name_msg, strlen(name_msg) + 1, 0);

                    char welcome_msg[BUFSIZE];
                    snprintf(welcome_msg, sizeof(welcome_msg), "チャットサーバに接続されました。");
                    send(connected_socket, welcome_msg, strlen(welcome_msg) + 1, 0);

                    // すべてのクライアントに新しいクライアントの参加を通知
                    char join_notification[256];
                    snprintf(join_notification, sizeof(join_notification), "\033[36m*** %s がチャットに参加しました ***\033[0m", clients[client_index].client_name);
                    for (int i = 0; i < MAX_CLIENTS; i++)
                    {
                        if (clients[i].socket_fd != UNUSED)
                        {
                            send(clients[i].socket_fd, join_notification, strlen(join_notification) + 1, 0);
                        }
                    }
                }
                else
                {
                    printf("クライアント数が上限に達しています\n");
                    close(connected_socket);
                }
            }
        }

        // 既存のクライアントからのデータをチェック
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].socket_fd != UNUSED && FD_ISSET(clients[i].socket_fd, &readfds))
            {
                memset(buffer, '\0', BUFSIZE);
                int bytes_received = recv(clients[i].socket_fd, buffer, BUFSIZE, 0);

                if (bytes_received <= 0 || strcmp(buffer, "quit") == 0)
                {
                    // クライアントが切断
                    FD_CLR(clients[i].socket_fd, &masterfds);
                    remove_client(clients, MAX_CLIENTS, i);
                }
                else
                {
                    // メッセージを処理
                    chop(buffer);
                    printf("[%s]: %s\n", clients[i].client_name, buffer);

                    // コマンドかチェック
                    if (!handle_commands(clients, MAX_CLIENTS, i, buffer))
                    {
                        // 通常のメッセージなので他のクライアントに転送
                        broadcast_message(clients, MAX_CLIENTS, i, buffer);
                    }
                }
            }
        }
    }

    close(listening_socket);
    return 0;
}
