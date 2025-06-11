#include <gtk/gtk.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 256

// グローバル変数
GtkWidget *text_view;
GtkWidget *entry;
int socket_fd;
char current_name[64] = "";

void chop(char *str)
{
    char *p = strchr(str, '\n');
    if (p != NULL)
        *p = '\0';
}

// メッセージをテキストビューに追加する関数
void append_message(const char *message)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message, -1);

    // イテレータを再取得
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
}

// 送信ボタンのイベントハンドラ
void on_send_button_clicked(GtkWidget *widget, gpointer data)
{
    const char *message = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(message) > 0)
    {
        // メッセージをサーバーに送信
        send(socket_fd, message, strlen(message) + 1, 0);

        // アスキーアートコマンドの場合はコマンドをテキストビューに表示しない
        if (!(strncmp(message, ":art ", 5) == 0))
        {
            // テキストビューにメッセージを追加
            char display_message[BUFSIZE];
            snprintf(display_message, sizeof(display_message), "[%s]: %s", current_name, message);
            append_message(display_message);
        }
        
        // 名前変更コマンドの場合はcurrent_nameを更新
        if (strncmp(message, ":name ", 6) == 0)
        {
            strncpy(current_name, message + 6, sizeof(current_name) - 1);
            current_name[sizeof(current_name) - 1] = '\0';
        }

        // テキストボックスを空にする
        gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
}

// 切断ボタンのイベントハンドラ
void on_disconnect_button_clicked(GtkWidget *widget, gpointer data)
{
    // quitを送信してサーバーとの接続を切断
    send(socket_fd, "quit", 5, 0);
    append_message("サーバーとの接続を切断しました。");
    gtk_main_quit();
    close(socket_fd);
    exit(0);
}

// サーバーからのメッセージを受信するスレッド
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
            append_message("サーバとの接続が切断されました。");
            break;
        }

        // サーバーからの初期名前設定メッセージをチェック
        if (strncmp(buffer, "INITIAL_NAME:", 13) == 0)
        {
            strcpy(current_name, buffer + 13);
            char name_msg[BUFSIZE];
            snprintf(name_msg, sizeof(name_msg), "あなたの名前は %s です。", current_name);
            append_message(name_msg);
            continue;
        }

        // bufferからエスケープシーケンスを除去(GTKでは表示されないため)
        char clean_buf[BUFSIZE];
        int j = 0;
        for (int i = 0; i < bytes_received; i++)
        {
            if (buffer[i] == '\033')
            {
                while (i < bytes_received && buffer[i] != 'm')
                {
                    i++;
                }
                if (i < bytes_received)
                    continue;
            }
            else
            {
                clean_buf[j++] = buffer[i];
            }
        }
        clean_buf[j] = '\0';
        // テキストビューにメッセージを追加
        append_message(clean_buf);
    }
    return NULL;
}

// 送信スレッド
void *send_thread(void *arg)
{
    // 少し待機してサーバーからの初期名前を受信
    sleep(1);

    append_message("利用可能なコマンドを表示するには ':help' を入力してください。");
    fflush(stdout);

    return NULL;
}

int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *send_button;
    GtkWidget *disconnect_button;
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

    // GTKの初期化
    gtk_init(&argc, &argv);

    // メインウィンドウの作成
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "簡易チャットアプリ");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // レイアウトの作成
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // テキストビューの作成
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    // 入力エリアと送信ボタンの作成
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    // 送信ボタンの作成
    send_button = gtk_button_new_with_label("送信");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_button_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), send_button, FALSE, FALSE, 0);

    // エンターキーで送信処理
    g_signal_connect(entry, "activate", G_CALLBACK(on_send_button_clicked), NULL);

    // 切断ボタンの作成
    disconnect_button = gtk_button_new_with_label("切断");
    g_signal_connect(disconnect_button, "clicked", G_CALLBACK(on_disconnect_button_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), disconnect_button, FALSE, FALSE, 0);

    // ウィンドウの閉じるボタンを押したときの処理
    g_signal_connect(window, "destroy", G_CALLBACK(on_disconnect_button_clicked), NULL);

    // サーバーからのメッセージを送受信するスレッドの作成
    pthread_t recv_tid, send_tid;
    if (pthread_create(&recv_tid, NULL, receive_thread, &socket_fd) != 0)
    {
        perror("Thread creation failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&send_tid, NULL, send_thread, &socket_fd) != 0)
    {
        perror("Thread creation failed (send)");
        exit(EXIT_FAILURE);
    }

    // ウィンドウの表示
    gtk_widget_show_all(window);
    gtk_main();

    // スレッドの終了を待機
    pthread_join(recv_tid, NULL);

    close(socket_fd);

    return 0;
}
