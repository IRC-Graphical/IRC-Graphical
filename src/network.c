#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "irc_client.h"

typedef struct {
    int server_idx;
    char message[MAX_MSG_LENGTH];
} gui_update_data_t;

int connect_to_server(server_info_t *server) {
    struct addrinfo hints, *result, *rp;
    char port_str[16];
    int status;

    server->state = CONN_CONNECTING;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    
    snprintf(port_str, sizeof(port_str), "%d", server->port);
    
    status = getaddrinfo(server->hostname, port_str, &hints, &result);
    if (status != 0) {
        log_message("ERROR", "getaddrinfo failed for %s: %s", 
                   server->hostname, gai_strerror(status));
        server->state = CONN_ERROR;
        return -1;
    }

    server->sockfd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server->sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server->sockfd == -1) continue;
        
        if (connect(server->sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            break;
        }
        
        close(server->sockfd);
        server->sockfd = -1;
    }

    freeaddrinfo(result);

    if (server->sockfd == -1) {
        log_message("ERROR", "Could not connect to %s:%d", server->hostname, server->port);
        server->state = CONN_ERROR;
        return -1;
    }

    server->state = CONN_CONNECTED;
    log_message("INFO", "Connected to %s:%d", server->hostname, server->port);
    
    // Send initial IRC commands
    char cmd[MAX_MSG_LENGTH];
    
    if (strlen(server->password) > 0) {
        snprintf(cmd, sizeof(cmd), "PASS %s\r\n", server->password);
        send_irc_command(server, cmd);
    }
    
    snprintf(cmd, sizeof(cmd), "NICK %s\r\n", server->nick);
    send_irc_command(server, cmd);
    
    snprintf(cmd, sizeof(cmd), "USER %s 0 * :%s\r\n", server->nick, server->real_name);
    send_irc_command(server, cmd);
    
    return server->sockfd;
}

void disconnect_server(server_info_t *server) {
    if (server->sockfd > 0) {
        send_irc_command(server, "QUIT :Client disconnecting\r\n");
        close(server->sockfd);
        server->sockfd = -1;
    }
    
    server->state = CONN_DISCONNECTED;
    
    if (server->network_thread) {
        pthread_cancel(server->network_thread);
        pthread_join(server->network_thread, NULL);
        server->network_thread = 0;
    }
    
    log_message("INFO", "Disconnected from %s", server->name);
}

void send_irc_command(server_info_t *server, const char *cmd) {
    if (server->state != CONN_CONNECTED || server->sockfd <= 0) {
        log_message("WARNING", "Attempted to send command to disconnected server");
        return;
    }
    
    ssize_t sent = send(server->sockfd, cmd, strlen(cmd), 0);
    if (sent < 0) {
        log_message("ERROR", "Failed to send command: %s", strerror(errno));
        server->state = CONN_ERROR;
    } else {
        log_message("DEBUG", "Sent: %s", cmd);
    }
}

gboolean gui_update_callback(gpointer data) {
    gui_update_data_t *update = (gui_update_data_t*)data;
    
    pthread_mutex_lock(&client.gui_mutex);
    
    // Find the appropriate channel and append message
    server_info_t *server = &client.servers[update->server_idx];
    
    // For now, just append to active channel or create a status channel
    if (server->active_channel >= 0) {
        append_message_to_channel(update->server_idx, server->active_channel, update->message);
    }
    
    pthread_mutex_unlock(&client.gui_mutex);
    
    free(update);
    return FALSE; // Don't repeat
}

void handle_irc_message(server_info_t *server, const char *message) {
    char *msg_copy = strdup(message);
    char *saveptr;
    char *prefix = NULL;
    char *command;
    char *params;
    
    log_message("DEBUG", "Received: %s", message);
    
    // Parse IRC message format: [:prefix] command [params]
    if (msg_copy[0] == ':') {
        prefix = strtok_r(msg_copy + 1, " ", &saveptr);
        command = strtok_r(NULL, " ", &saveptr);
    } else {
        command = strtok_r(msg_copy, " ", &saveptr);
    }
    
    if (!command) {
        free(msg_copy);
        return;
    }
    
    params = saveptr;
    
    // Handle different IRC commands
    if (strcmp(command, "PING") == 0) {
        char pong[MAX_MSG_LENGTH];
        snprintf(pong, sizeof(pong), "PONG %s\r\n", params ? params : "");
        send_irc_command(server, pong);
    }
    else if (strcmp(command, "PRIVMSG") == 0) {
        char *target = strtok_r(NULL, " ", &saveptr);
        char *msg_text = saveptr;
        
        if (msg_text && msg_text[0] == ':') msg_text++;
        
        if (target && msg_text && prefix) {
            char *nick = strtok(prefix, "!");
            char display_msg[MAX_MSG_LENGTH];
            
            // Check if it's a private message to us
            if (strcmp(target, server->nick) == 0) {
                // Private message - find or create DM channel
                int dm_channel = -1;
                for (int i = 0; i < server->channel_count; i++) {
                    if (server->channels[i].is_private_msg && 
                        strcmp(server->channels[i].target_nick, nick) == 0) {
                        dm_channel = i;
                        break;
                    }
                }
                
                if (dm_channel == -1) {
                    int server_idx = server - client.servers;
                    add_channel_to_server(server_idx, nick, true);
                    dm_channel = server->channel_count - 1;
                    strcpy(server->channels[dm_channel].target_nick, nick);
                }
                
                snprintf(display_msg, sizeof(display_msg), "%s <%s> %s\n", 
                         get_timestamp(), nick, msg_text);
                
                gui_update_data_t *update = malloc(sizeof(gui_update_data_t));
                update->server_idx = server - client.servers;
                strcpy(update->message, display_msg);
                g_idle_add(gui_update_callback, update);
            } else {
                // Channel message
                snprintf(display_msg, sizeof(display_msg), "%s <%s> %s\n", 
                         get_timestamp(), nick, msg_text);
                
                // Find the channel
                for (int i = 0; i < server->channel_count; i++) {
                    if (!server->channels[i].is_private_msg && 
                        (strcmp(server->channels[i].name, target) == 0 ||
                         (target[0] == '#' && strcmp(server->channels[i].name, target + 1) == 0))) {
                        
                        gui_update_data_t *update = malloc(sizeof(gui_update_data_t));
                        update->server_idx = server - client.servers;
                        strcpy(update->message, display_msg);
                        g_idle_add(gui_update_callback, update);
                        break;
                    }
                }
            }
        }
    }
    else if (strcmp(command, "001") == 0) {
        // Welcome message - we're successfully connected
        log_message("INFO", "Successfully logged into %s", server->name);
        update_status("Connected and logged in");
        
        // Auto-join channels if any are configured
        // This could be expanded to load from config
    }
    else if (strcmp(command, "JOIN") == 0) {
        char *channel = strtok_r(NULL, " ", &saveptr);
        if (channel && prefix) {
            char *nick = strtok(prefix, "!");
            if (strcmp(nick, server->nick) == 0) {
                // We joined a channel
                if (channel[0] == ':') channel++;
                if (channel[0] == '#') channel++;
                
                int server_idx = server - client.servers;
                add_channel_to_server(server_idx, channel, false);
                log_message("INFO", "Joined channel #%s", channel);
            }
        }
    }
    else if (strcmp(command, "PART") == 0 || strcmp(command, "KICK") == 0) {
        // Handle leaving channels
        char *channel = strtok_r(NULL, " ", &saveptr);
        if (channel && prefix) {
            char *nick = strtok(prefix, "!");
            if (strcmp(nick, server->nick) == 0) {
                // We left a channel - remove it from our list
                if (channel[0] == ':') channel++;
                if (channel[0] == '#') channel++;
                
                for (int i = 0; i < server->channel_count; i++) {
                    if (!server->channels[i].is_private_msg && 
                        strcmp(server->channels[i].name, channel) == 0) {
                        // Remove channel (shift array)
                        for (int j = i; j < server->channel_count - 1; j++) {
                            server->channels[j] = server->channels[j + 1];
                        }
                        server->channel_count--;
                        if (server->active_channel >= i) {
                            server->active_channel--;
                        }
                        break;
                    }
                }
                log_message("INFO", "Left channel #%s", channel);
            }
        }
    }
    
    free(msg_copy);
}

void* network_thread_func(void* arg) {
    int server_idx = *(int*)arg;
    server_info_t *server = &client.servers[server_idx];
    char buffer[MAX_MSG_LENGTH];
    char line_buffer[MAX_MSG_LENGTH * 2]; // For handling partial lines
    int line_pos = 0;
    
    while (client.running && server->state == CONN_CONNECTED) {
        ssize_t bytes_received = recv(server->sockfd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            if (bytes_received < 0) {
                log_message("ERROR", "recv() error: %s", strerror(errno));
            } else {
                log_message("INFO", "Server closed connection");
            }
            server->state = CONN_ERROR;
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Handle partial lines
        for (int i = 0; i < bytes_received; i++) {
            if (buffer[i] == '\n') {
                line_buffer[line_pos] = '\0';
                if (line_pos > 0 && line_buffer[line_pos - 1] == '\r') {
                    line_buffer[line_pos - 1] = '\0';
                }
                
                if (strlen(line_buffer) > 0) {
                    handle_irc_message(server, line_buffer);
                }
                line_pos = 0;
            } else if (line_pos < sizeof(line_buffer) - 1) {
                line_buffer[line_pos++] = buffer[i];
            }
        }
    }
    
    log_message("INFO", "Network thread for %s terminated", server->name);
    server->network_thread = 0;
    return NULL;
}
