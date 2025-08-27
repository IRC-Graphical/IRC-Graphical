#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <json-c/json.h>
#include "client.h"

void load_config(void) {
    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        log_message("INFO", "No configuration file found, starting fresh");
        return;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return;
    }
    
    // Read file content
    char *json_string = malloc(file_size + 1);
    size_t read_size = fread(json_string, 1, file_size, file);
    json_string[read_size] = '\0';
    fclose(file);
    
    // Parse JSON
    json_object *root = json_tokener_parse(json_string);
    free(json_string);
    
    if (!root) {
        log_message("ERROR", "Failed to parse configuration file");
        return;
    }
    
    // Load servers array
    json_object *servers_array;
    if (json_object_object_get_ex(root, "servers", &servers_array)) {
        int array_len = json_object_array_length(servers_array);
        
        for (int i = 0; i < array_len && i < MAX_SERVERS; i++) {
            json_object *server_obj = json_object_array_get_idx(servers_array, i);
            if (!server_obj) continue;
            
            server_info_t *server = &client.servers[client.server_count];
            memset(server, 0, sizeof(server_info_t));
            
            // Load server properties
            json_object *prop;
            
            if (json_object_object_get_ex(server_obj, "name", &prop)) {
                strncpy(server->name, json_object_get_string(prop), MAX_SERVER_NAME - 1);
            }
            
            if (json_object_object_get_ex(server_obj, "hostname", &prop)) {
                strncpy(server->hostname, json_object_get_string(prop), INET6_ADDRSTRLEN - 1);
            }
            
            if (json_object_object_get_ex(server_obj, "port", &prop)) {
                server->port = json_object_get_int(prop);
            } else {
                server->port = 6667;
            }
            
            if (json_object_object_get_ex(server_obj, "nick", &prop)) {
                strncpy(server->nick, json_object_get_string(prop), MAX_NICK_LENGTH - 1);
            }
            
            if (json_object_object_get_ex(server_obj, "real_name", &prop)) {
                strncpy(server->real_name, json_object_get_string(prop), 63);
            }
            
            if (json_object_object_get_ex(server_obj, "password", &prop)) {
                strncpy(server->password, json_object_get_string(prop), 63);
            }
            
            if (json_object_object_get_ex(server_obj, "auto_connect", &prop)) {
                server->auto_connect = json_object_get_boolean(prop);
            }
            
            server->state = CONN_DISCONNECTED;
            server->sockfd = -1;
            server->active_channel = -1;
            
            // Load channels for this server
            json_object *channels_array;
            if (json_object_object_get_ex(server_obj, "channels", &channels_array)) {
                int channels_len = json_object_array_length(channels_array);
                
                for (int j = 0; j < channels_len && j < MAX_CHANNELS_PER_SERVER; j++) {
                    json_object *channel_obj = json_object_array_get_idx(channels_array, j);
                    if (!channel_obj) continue;
                    
                    channel_info_t *channel = &server->channels[server->channel_count];
                    memset(channel, 0, sizeof(channel_info_t));
                    
                    if (json_object_object_get_ex(channel_obj, "name", &prop)) {
                        strncpy(channel->name, json_object_get_string(prop), MAX_CHANNEL_LENGTH - 1);
                    }
                    
                    if (json_object_object_get_ex(channel_obj, "is_private_msg", &prop)) {
                        channel->is_private_msg = json_object_get_boolean(prop);
                    }
                    
                    if (channel->is_private_msg && json_object_object_get_ex(channel_obj, "target_nick", &prop)) {
                        strncpy(channel->target_nick, json_object_get_string(prop), MAX_NICK_LENGTH - 1);
                    }
                    
                    if (json_object_object_get_ex(channel_obj, "auto_join", &prop)) {
                        channel->active = json_object_get_boolean(prop);
                    }
                    
                    channel->buffer = gtk_text_buffer_new(NULL);
                    server->channel_count++;
                }
            }
            
            client.server_count++;
        }
    }
    
    json_object_put(root);
    
    log_message("INFO", "Loaded configuration: %d servers", client.server_count);
}

void save_config(void) {
    json_object *root = json_object_new_object();
    json_object *servers_array = json_object_new_array();
    
    for (int i = 0; i < client.server_count; i++) {
        server_info_t *server = &client.servers[i];
        json_object *server_obj = json_object_new_object();
        
        json_object_object_add(server_obj, "name", json_object_new_string(server->name));
        json_object_object_add(server_obj, "hostname", json_object_new_string(server->hostname));
        json_object_object_add(server_obj, "port", json_object_new_int(server->port));
        json_object_object_add(server_obj, "nick", json_object_new_string(server->nick));
        json_object_object_add(server_obj, "real_name", json_object_new_string(server->real_name));
        
        if (strlen(server->password) > 0) {
            json_object_object_add(server_obj, "password", json_object_new_string(server->password));
        }
        
        json_object_object_add(server_obj, "auto_connect", json_object_new_boolean(server->auto_connect));
        
        // Save channels
        json_object *channels_array = json_object_new_array();
        for (int j = 0; j < server->channel_count; j++) {
            channel_info_t *channel = &server->channels[j];
            json_object *channel_obj = json_object_new_object();
            
            json_object_object_add(channel_obj, "name", json_object_new_string(channel->name));
            json_object_object_add(channel_obj, "is_private_msg", json_object_new_boolean(channel->is_private_msg));
            json_object_object_add(channel_obj, "auto_join", json_object_new_boolean(channel->active));
            
            if (channel->is_private_msg) {
                json_object_object_add(channel_obj, "target_nick", json_object_new_string(channel->target_nick));
            }
            
            json_object_array_add(channels_array, channel_obj);
        }
        
        json_object_object_add(server_obj, "channels", channels_array);
        json_object_array_add(servers_array, server_obj);
    }
    
    json_object_object_add(root, "servers", servers_array);
    
    // Write to file
    FILE *file = fopen(CONFIG_FILE, "w");
    if (file) {
        const char *json_string = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
        fprintf(file, "%s\n", json_string);
        fclose(file);
        log_message("INFO", "Configuration saved");
    } else {
        log_message("ERROR", "Failed to save configuration file");
    }
    
    json_object_put(root);
}
