#ifndef IRC_CLIENT_H
#define IRC_CLIENT_H

#include <gtk/gtk.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdbool.h>

#define MAX_MSG_LENGTH 512
#define MAX_NICK_LENGTH 32
#define MAX_CHANNEL_LENGTH 64
#define MAX_SERVER_NAME 128
#define MAX_SERVERS 16
#define MAX_CHANNELS_PER_SERVER 32
#define CONFIG_FILE "irc_config.json"

typedef enum {
    CONN_DISCONNECTED,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_ERROR
} connection_state_t;

typedef struct {
    char name[MAX_CHANNEL_LENGTH];
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    bool active;
    bool is_private_msg;
    char target_nick[MAX_NICK_LENGTH]; // For private messages
} channel_info_t;

typedef struct {
    char name[MAX_SERVER_NAME];
    char hostname[INET6_ADDRSTRLEN];
    int port;
    char nick[MAX_NICK_LENGTH];
    char real_name[64];
    char password[64]; // Optional server password
    
    int sockfd;
    connection_state_t state;
    pthread_t network_thread;
    
    channel_info_t channels[MAX_CHANNELS_PER_SERVER];
    int channel_count;
    int active_channel;
    
    GtkWidget *server_item;
    GtkWidget *channel_list;
    bool auto_connect;
} server_info_t;

typedef struct {
    GtkWidget *window;
    GtkWidget *main_paned;
    GtkWidget *server_list;
    GtkWidget *channel_paned;
    GtkWidget *channel_list;
    GtkWidget *chat_area;
    GtkWidget *message_entry;
    GtkWidget *user_list;
    GtkWidget *status_bar;
    
    server_info_t servers[MAX_SERVERS];
    int server_count;
    int active_server;
    
    pthread_mutex_t gui_mutex;
    bool running;
} irc_client_t;

// Global client instance
extern irc_client_t client;

// Function prototypes
void init_client(void);
void cleanup_client(void);
void load_config(void);
void save_config(void);

// Network functions
void* network_thread_func(void* arg);
int connect_to_server(server_info_t *server);
void disconnect_server(server_info_t *server);
void send_irc_command(server_info_t *server, const char *cmd);
void handle_irc_message(server_info_t *server, const char *message);

// GUI functions
void create_main_window(void);
void add_server_dialog(void);
void connect_to_server_gui(int server_idx);
void add_channel_to_server(int server_idx, const char *channel_name, bool is_dm);
void switch_to_channel(int server_idx, int channel_idx);
void append_message_to_channel(int server_idx, int channel_idx, const char *message);
void update_status(const char *message);
void update_channel_list(int server_idx);
void show_user_list(int server_idx, const char *channel);

// Utility functions
char* get_timestamp(void);
void log_message(const char *level, const char *format, ...);
gboolean gui_update_callback(gpointer data);

// GUI Callbacks
void on_window_destroy(GtkWidget *widget, gpointer data);
void on_message_entry_activate(GtkEntry *entry, gpointer data);
void on_server_connect_clicked(GtkButton *button, gpointer data);
void on_add_server_clicked(GtkButton *button, gpointer data);
void on_channel_selection_changed(GtkTreeSelection *selection, gpointer data);

#endif
