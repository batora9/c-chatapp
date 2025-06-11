#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFSIZE 256
#define MAX_CLIENTS 10
#define UNUSED (-1)

// クライアント管理構造体
typedef struct
{
    int socket_fd;
    struct sockaddr_in addr;
    char client_name[64];
} client_info_t;

// コマンド処理関数の宣言
int handle_name_command(client_info_t clients[], int max_clients, int client_index, const char *new_name);
int handle_whoami_command(client_info_t clients[], int client_index);
int handle_members_command(client_info_t clients[], int max_clients, int client_index);
int handle_commands(client_info_t clients[], int max_clients, int client_index, const char *message);
int emoji_command(client_info_t clients[], int max_clients, int client_index, const char *message);
int handle_help_command(int client_socket_fd);

// コマンド処理関数の実装

// 名前変更コマンドの処理
int handle_name_command(client_info_t clients[], int max_clients, int client_index, const char *new_name)
{
    // 空白を除去
    while (*new_name == ' ')
        new_name++;

    if (strlen(new_name) == 0)
    {
        char error_msg[] = "\033[31mエラー: 名前が空です。使用方法: :name <新しい名前>\033[0m";
        send(clients[client_index].socket_fd, error_msg, strlen(error_msg) + 1, 0);
        return 1;
    }

    if (strlen(new_name) >= sizeof(clients[client_index].client_name))
    {
        char error_msg[] = "\033[31mエラー: 名前が長すぎます。\033[0m";
        send(clients[client_index].socket_fd, error_msg, strlen(error_msg) + 1, 0);
        return 1;
    }

    // 名前の重複チェック
    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].socket_fd != UNUSED && i != client_index &&
            strcmp(clients[i].client_name, new_name) == 0)
        {
            char error_msg[] = "\033[31mエラー: その名前は既に使用されています。\033[0m";
            send(clients[client_index].socket_fd, error_msg, strlen(error_msg) + 1, 0);
            return 1;
        }
    }

    char old_name[64];
    strcpy(old_name, clients[client_index].client_name);
    strcpy(clients[client_index].client_name, new_name);

    // 成功メッセージを送信者に送信
    char success_msg[128];
    snprintf(success_msg, sizeof(success_msg), "\033[90m名前を '%s' に変更しました。\033[0m", new_name);
    send(clients[client_index].socket_fd, success_msg, strlen(success_msg) + 1, 0);

    // 名前変更を他のクライアントに通知
    char notification[256];
    snprintf(notification, sizeof(notification), "\033[33m*** %s が %s に名前を変更しました ***\033[0m", old_name, new_name);
    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].socket_fd != UNUSED && i != client_index)
        {
            send(clients[i].socket_fd, notification, strlen(notification) + 1, 0);
        }
    }

    printf("\033[34mクライアント %s が %s に名前を変更しました\033[0m\n", old_name, new_name);
    return 1;
}

// 現在の名前を取得するコマンドの処理
int handle_whoami_command(client_info_t clients[], int client_index)
{
    char response[128];
    snprintf(response, sizeof(response), "あなたの名前は '%s' です。", clients[client_index].client_name);
    send(clients[client_index].socket_fd, response, strlen(response) + 1, 0);
    return 1;
}

// 参加中のメンバー一覧を表示するコマンドの処理
int handle_members_command(client_info_t clients[], int max_clients, int client_index)
{
    char response[BUFSIZE * 2];
    strcpy(response, "\033[32m参加中のメンバー:\033[0m\n");

    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].socket_fd != UNUSED)
        {
            char member_line[128];
            if (i == client_index)
            {
                snprintf(member_line, sizeof(member_line), "  %s (あなた)\n", clients[i].client_name);
            }
            else
            {
                snprintf(member_line, sizeof(member_line), "  %s\n", clients[i].client_name);
            }
            strcat(response, member_line);
        }
    }

    send(clients[client_index].socket_fd, response, strlen(response) + 1, 0);
    return 1;
}

// アスキーアートを送信するコマンドの処理
int handle_ascii_art_command(client_info_t clients[], int max_client, int client_index, const char *art_name)
{
    const char *ascii_art = NULL;

    // アスキーアートの選択
    if (strcmp(art_name, "smile") == 0)
    {
        ascii_art = "^_^";
    }
    else if (strcmp(art_name, "heart") == 0)
    {
        ascii_art = "(♥_♥)";
    }
    else if (strcmp(art_name, "cat") == 0)
    {
        ascii_art = "(=^_^=)";
    }
    else if (strcmp(art_name, "cry") == 0)
    {
        ascii_art = "(╥_╥)";
    }
    else if (strcmp(art_name, "fish") == 0)
    {
        ascii_art = "<°)))><";
    }
    else if (strcmp(art_name, "tableflip") == 0)
    {
        ascii_art = "(╯°□°）╯︵ ┻━┻";
    }
    else if (strcmp(art_name, "hi") == 0)
    {
        ascii_art = "(・∀・)ノ";
    }
    else if (strcmp(art_name, "thinking") == 0)
    {
        ascii_art = "(0.o?)";
    }
    else if (strcmp(art_name, "gimme") == 0)
    {
        ascii_art = "༼つ ◕_◕ ༽つ";
    }
    else if (strcmp(art_name, "wizard") == 0)
    {
        ascii_art = "('-')⊃━☆ﾟ.*･｡ﾟ";
    }
    else
    {
        char error_msg[] = "\033[31mエラー: 無効なアート名です。使用可能なアート:\n "
                           "smile, heart, cat, cry, fish, tableflip, hi, thinking, gimme, wizard\033[0m";
        send(clients[client_index].socket_fd, error_msg, strlen(error_msg) + 1, 0);
        return 1;
    }

    // アスキーアートを全クライアントに送信
    char art_message[BUFSIZE + 128];
    snprintf(art_message, sizeof(art_message), "[%s]:%s", clients[client_index].client_name, ascii_art);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].socket_fd != UNUSED)
        {
            send(clients[i].socket_fd, art_message, strlen(art_message) + 1, 0);
        }
    }
    return 1;
}

// ヘルプコマンドの処理
int handle_help_command(int client_socket_fd)
{
    const char *help_message =
        "利用可能なコマンド:\n"
        "  :name <新しい名前> - 名前を変更\n"
        "  :whoami - 現在の名前を表示\n"
        "  :members - 参加中のメンバー一覧を表示\n"
        "  :help - このヘルプを表示\n"
        "  :art <アート名> - アスキーアートを送信\n"
        "    利用可能なアート: smile, heart, cat, cry, fish, tableflip, hi, thinking, gimme, wizard\n"
        "  quit - チャットを終了\n";

    send(client_socket_fd, help_message, strlen(help_message) + 1, 0);
    return 1;
}

// メインのコマンド処理関数
int handle_commands(client_info_t clients[], int max_clients, int client_index, const char *message)
{
    // 名前変更コマンド
    if (strncmp(message, ":name ", 6) == 0)
    {
        const char *new_name = message + 6;
        return handle_name_command(clients, max_clients, client_index, new_name);
    }

    // 現在の名前を取得するコマンド
    if (strcmp(message, ":whoami") == 0)
    {
        return handle_whoami_command(clients, client_index);
    }

    // 参加中のメンバー一覧を表示するコマンド
    if (strcmp(message, ":members") == 0)
    {
        return handle_members_command(clients, max_clients, client_index);
    }

    // アスキーアート送信コマンド
    if (strncmp(message, ":art ", 5) == 0)
    {
        const char *art_name = message + 5;
        return handle_ascii_art_command(clients, max_clients, client_index, art_name);
    }

    // ヘルプコマンド
    if (strcmp(message, ":help") == 0)
    {
        return handle_help_command(clients[client_index].socket_fd);
    }

    return 0; // 通常のメッセージ
}

#endif