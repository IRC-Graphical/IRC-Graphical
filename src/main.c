#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <json-c/json.h>
#include "client.h"

client_t client;

void init_client(void) {
    memset(&client, 0, sizeof(client));
    client.active_server = -1;
    client.running = true;
    pthread_mutex_init(&client.gui_mutex, NULL);
}

void cleanup_client(void) {
    client.running = false;
    
    for (int i = 0; i < client.server_count; i++) {
        disconnect_server(&client.servers[i]);
    }
    
    pthread_mutex_destroy(&client.gui_mutex);
    save_config();
}

char* get_timestamp(void) {
    static char timestamp[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S]", tm_info);
    return timestamp;
}

void log_message(const char *level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    printf("%s [%s] ", get_timestamp(), level);
    vprintf(format, args);
    printf("\n");
    
    va_end(args);
}

int main(int argc, char *argv[]) {
    // Initialize GTK
    gtk_init(&argc, &argv);
    
    // Initialize client
    init_client();
    
    // Load configuration
    load_config();
    
    // Create main window
    create_main_window();
    
    // Set up signal handlers
    signal(SIGTERM, (void(*)(int))cleanup_client);
    signal(SIGINT, (void(*)(int))cleanup_client);
    
    // Run GTK main loop
    gtk_main();
    
    // Cleanup
    cleanup_client();
    
    return 0;
}

void on_window_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    cleanup_client();
    gtk_main_quit();
}

void create_main_window(void) {
    GtkWidget *vbox, *hbox, *toolbar, *scrolled;
    GtkWidget *add_server_btn, *connect_btn;
    GtkTreeStore *server_store;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // Create main window
    client.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(client.window), "IRC Client");
    gtk_window_set_default_size(GTK_WINDOW(client.window), 1200, 800);
    gtk_container_set_border_width(GTK_CONTAINER(client.window), 5);
    
    g_signal_connect(client.window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // Create main vertical box
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(client.window), vbox);
    
    // Create toolbar
    toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    
    add_server_btn = gtk_button_new_with_label("Add Server");
    connect_btn = gtk_button_new_with_label("Connect");
    
    gtk_box_pack_start(GTK_BOX(toolbar), add_server_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), connect_btn, FALSE, FALSE, 0);
    
    g_signal_connect(add_server_btn, "clicked", G_CALLBACK(on_add_server_clicked), NULL);
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_server_connect_clicked), NULL);
    
    // Create main horizontal paned window
    client.main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), client.main_paned, TRUE, TRUE, 0);
    
    // Create server list (left panel)
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, 200, -1);
    
    server_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    client.server_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(server_store));
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Servers", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(client.server_list), column);
    
    gtk_container_add(GTK_CONTAINER(scrolled), client.server_list);
    gtk_paned_pack1(GTK_PANED(client.main_paned), scrolled, FALSE, TRUE);
    
    // Create channel paned window (right side)
    client.channel_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack2(GTK_PANED(client.main_paned), client.channel_paned, TRUE, TRUE);
    
    // Create channel list
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, 150, -1);
    
    client.channel_list = gtk_tree_view_new();
    gtk_container_add(GTK_CONTAINER(scrolled), client.channel_list);
    gtk_paned_pack1(GTK_PANED(client.channel_paned), scrolled, FALSE, TRUE);
    
    // Create chat area paned window
    GtkWidget *chat_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack2(GTK_PANED(client.channel_paned), chat_paned, TRUE, TRUE);
    
    // Create chat area
    GtkWidget *chat_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_paned_pack1(GTK_PANED(chat_paned), chat_vbox, TRUE, TRUE);
    
    // Chat text view
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    
    client.chat_area = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(client.chat_area), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(client.chat_area), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(client.chat_area), GTK_WRAP_WORD);
    
    gtk_container_add(GTK_CONTAINER(scrolled), client.chat_area);
    gtk_box_pack_start(GTK_BOX(chat_vbox), scrolled, TRUE, TRUE, 0);
    
    // Message entry
    client.message_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(client.message_entry), "Type a message...");
    gtk_box_pack_start(GTK_BOX(chat_vbox), client.message_entry, FALSE, FALSE, 0);
    
    g_signal_connect(client.message_entry, "activate", G_CALLBACK(on_message_entry_activate), NULL);
    
    // Create user list (right panel)
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, 120, -1);
    
    client.user_list = gtk_tree_view_new();
    gtk_container_add(GTK_CONTAINER(scrolled), client.user_list);
    gtk_paned_pack2(GTK_PANED(chat_paned), scrolled, FALSE, TRUE);
    
    // Create status bar
    client.status_bar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), client.status_bar, FALSE, FALSE, 0);
    
    update_status("Ready");
    
    // Show all widgets
    gtk_widget_show_all(client.window);
}

void on_message_entry_activate(GtkEntry *entry, gpointer data) {
    (void)data;
    
    if (client.active_server < 0) return;
    
    server_info_t *server = &client.servers[client.active_server];
    if (server->active_channel < 0) return;
    
    const char *message = gtk_entry_get_text(entry);
    if (strlen(message) == 0) return;
    
    channel_info_t *channel = &server->channels[server->active_channel];
    char cmd[MAX_MSG_LENGTH];
    
    if (message[0] == '/') {
        // Handle commands
        if (strncmp(message, "/join ", 6) == 0) {
            snprintf(cmd, sizeof(cmd), "JOIN %s\r\n", message + 6);
            send_irc_command(server, cmd);
        } else if (strncmp(message, "/quit", 5) == 0) {
            disconnect_server(server);
        } else if (strncmp(message, "/msg ", 5) == 0) {
            // Private message: /msg nick message
            char *space = strchr(message + 5, ' ');
            if (space) {
                *space = '\0';
                const char *nick = message + 5;
                const char *msg = space + 1;
                
                snprintf(cmd, sizeof(cmd), "PRIVMSG %s :%s\r\n", nick, msg);
                send_irc_command(server, cmd);
                
                // Add to DM channel or create it
                int dm_channel = -1;
                for (int i = 0; i < server->channel_count; i++) {
                    if (server->channels[i].is_private_msg && 
                        strcmp(server->channels[i].target_nick, nick) == 0) {
                        dm_channel = i;
                        break;
                    }
                }
                
                if (dm_channel == -1) {
                    add_channel_to_server(client.active_server, nick, true);
                    dm_channel = server->channel_count - 1;
                    strcpy(server->channels[dm_channel].target_nick, nick);
                }
                
                char display_msg[MAX_MSG_LENGTH];
                snprintf(display_msg, sizeof(display_msg), "%s <%s> %s\n", 
                         get_timestamp(), server->nick, msg);
                append_message_to_channel(client.active_server, dm_channel, display_msg);
            }
        } else {
            // Send raw command
            snprintf(cmd, sizeof(cmd), "%s\r\n", message + 1);
            send_irc_command(server, cmd);
        }
    } else {
        // Regular message
        if (channel->is_private_msg) {
            snprintf(cmd, sizeof(cmd), "PRIVMSG %s :%s\r\n", channel->target_nick, message);
        } else {
            snprintf(cmd, sizeof(cmd), "PRIVMSG #%s :%s\r\n", channel->name, message);
        }
        send_irc_command(server, cmd);
        
        // Echo message to chat
        char display_msg[MAX_MSG_LENGTH];
        snprintf(display_msg, sizeof(display_msg), "%s <%s> %s\n", 
                 get_timestamp(), server->nick, message);
        append_message_to_channel(client.active_server, server->active_channel, display_msg);
    }
    
    gtk_entry_set_text(entry, "");
}

void update_status(const char *message) {
    if (client.status_bar) {
        gtk_statusbar_remove_all(GTK_STATUSBAR(client.status_bar), 1);
        gtk_statusbar_push(GTK_STATUSBAR(client.status_bar), 1, message);
    }
}
