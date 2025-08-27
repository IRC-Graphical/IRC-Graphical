#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "irc_client.h"

void add_server_dialog(void) {
    GtkWidget *dialog, *content_area, *grid;
    GtkWidget *name_entry, *hostname_entry, *port_entry, *nick_entry, *realname_entry, *password_entry;
    GtkWidget *autoconnect_check;
    GtkWidget *name_label, *hostname_label, *port_label, *nick_label, *realname_label, *password_label;
    
    dialog = gtk_dialog_new_with_buttons("Add Server",
                                         GTK_WINDOW(client.window),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Add", GTK_RESPONSE_ACCEPT,
                                         NULL);
    
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    
    // Create form fields
    name_label = gtk_label_new("Server Name:");
    gtk_widget_set_halign(name_label, GTK_ALIGN_END);
    name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "My IRC Server");
    
    hostname_label = gtk_label_new("Hostname:");
    gtk_widget_set_halign(hostname_label, GTK_ALIGN_END);
    hostname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(hostname_entry), "irc.libera.chat");
    
    port_label = gtk_label_new("Port:");
    gtk_widget_set_halign(port_label, GTK_ALIGN_END);
    port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(port_entry), "6667");
    
    nick_label = gtk_label_new("Nickname:");
    gtk_widget_set_halign(nick_label, GTK_ALIGN_END);
    nick_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(nick_entry), "myuser");
    
    realname_label = gtk_label_new("Real Name:");
    gtk_widget_set_halign(realname_label, GTK_ALIGN_END);
    realname_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(realname_entry), "My Real Name");
    
    password_label = gtk_label_new("Password:");
    gtk_widget_set_halign(password_label, GTK_ALIGN_END);
    password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "(optional)");
    
    autoconnect_check = gtk_check_button_new_with_label("Auto-connect on startup");
    
    // Add to grid
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hostname_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hostname_entry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), port_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), port_entry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), nick_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), nick_entry, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), realname_label, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), realname_entry, 1, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_label, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), password_entry, 1, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), autoconnect_check, 0, 6, 2, 1);
    
    gtk_widget_show_all(dialog);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_ACCEPT && client.server_count < MAX_SERVERS) {
        server_info_t *server = &client.servers[client.server_count];
        memset(server, 0, sizeof(server_info_t));
        
        strncpy(server->name, gtk_entry_get_text(GTK_ENTRY(name_entry)), MAX_SERVER_NAME - 1);
        strncpy(server->hostname, gtk_entry_get_text(GTK_ENTRY(hostname_entry)), INET6_ADDRSTRLEN - 1);
        server->port = atoi(gtk_entry_get_text(GTK_ENTRY(port_entry)));
        if (server->port <= 0) server->port = 6667;
        
        strncpy(server->nick, gtk_entry_get_text(GTK_ENTRY(nick_entry)), MAX_NICK_LENGTH - 1);
        strncpy(server->real_name, gtk_entry_get_text(GTK_ENTRY(realname_entry)), 63);
        strncpy(server->password, gtk_entry_get_text(GTK_ENTRY(password_entry)), 63);
        
        server->auto_connect = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autoconnect_check));
        server->state = CONN_DISCONNECTED;
        server->sockfd = -1;
        server->active_channel = -1;
        
        // Add to server list
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(client.server_list));
        GtkTreeIter iter;
        
        gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
        gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 
                           0, server->name,
                           1, client.server_count,
                           -1);
        
        client.server_count++;
        save_config();
        
        char status_msg[256];
        snprintf(status_msg, sizeof(status_msg), "Added server: %s", server->name);
        update_status(status_msg);
    }
    
    gtk_widget_destroy(dialog);
}

void on_add_server_clicked(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    add_server_dialog();
}

void on_server_connect_clicked(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(client.server_list));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        int server_idx;
        gtk_tree_model_get(model, &iter, 1, &server_idx, -1);
        connect_to_server_gui(server_idx);
    } else {
        update_status("Please select a server to connect");
    }
}

void connect_to_server_gui(int server_idx) {
    if (server_idx < 0 || server_idx >= client.server_count) return;
    
    server_info_t *server = &client.servers[server_idx];
    
    if (server->state == CONN_CONNECTED) {
        update_status("Already connected to this server");
        return;
    }
    
    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg), "Connecting to %s...", server->name);
    update_status(status_msg);
    
    if (connect_to_server(server) >= 0) {
        client.active_server = server_idx;
        
        // Start network thread
        int *server_idx_ptr = malloc(sizeof(int));
        *server_idx_ptr = server_idx;
        
        if (pthread_create(&server->network_thread, NULL, network_thread_func, server_idx_ptr) != 0) {
            log_message("ERROR", "Failed to create network thread");
            disconnect_server(server);
            free(server_idx_ptr);
            return;
        }
        
        // Update channel list for this server
        update_channel_list(server_idx);
        
        snprintf(status_msg, sizeof(status_msg), "Connected to %s", server->name);
        update_status(status_msg);
    } else {
        update_status("Failed to connect");
    }
}

void add_channel_to_server(int server_idx, const char *channel_name, bool is_dm) {
    if (server_idx < 0 || server_idx >= client.server_count) return;
    
    server_info_t *server = &client.servers[server_idx];
    if (server->channel_count >= MAX_CHANNELS_PER_SERVER) return;
    
    // Check if channel already exists
    for (int i = 0; i < server->channel_count; i++) {
        if (server->channels[i].is_private_msg == is_dm) {
            if (is_dm) {
                if (strcmp(server->channels[i].target_nick, channel_name) == 0) return;
            } else {
                if (strcmp(server->channels[i].name, channel_name) == 0) return;
            }
        }
    }
    
    channel_info_t *channel = &server->channels[server->channel_count];
    memset(channel, 0, sizeof(channel_info_t));
    
    strncpy(channel->name, channel_name, MAX_CHANNEL_LENGTH - 1);
    channel->is_private_msg = is_dm;
    channel->active = true;
    
    // Create text buffer for this channel
    channel->buffer = gtk_text_buffer_new(NULL);
    
    server->channel_count++;
    
    // Update GUI
    update_channel_list(server_idx);
    
    log_message("INFO", "Added %s %s to server %s", 
               is_dm ? "DM" : "channel", channel_name, server->name);
}

void update_channel_list(int server_idx) {
    if (server_idx != client.active_server) return;
    
    server_info_t *server = &client.servers[server_idx];
    
    // Clear existing model
    GtkTreeStore *store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    gtk_tree_view_set_model(GTK_TREE_VIEW(client.channel_list), GTK_TREE_MODEL(store));
    
    // Add channels
    GtkTreeIter channels_iter, dm_iter, iter;
    gtk_tree_store_append(store, &channels_iter, NULL);
    gtk_tree_store_set(store, &channels_iter, 0, "Channels", 1, -1, -1);
    
    gtk_tree_store_append(store, &dm_iter, NULL);
    gtk_tree_store_set(store, &dm_iter, 0, "Direct Messages", 1, -1, -1);
    
    for (int i = 0; i < server->channel_count; i++) {
        channel_info_t *channel = &server->channels[i];
        GtkTreeIter *parent = channel->is_private_msg ? &dm_iter : &channels_iter;
        
        gtk_tree_store_append(store, &iter, parent);
        char display_name[MAX_CHANNEL_LENGTH + 2];
        if (channel->is_private_msg) {
            snprintf(display_name, sizeof(display_name), "@%s", channel->target_nick);
        } else {
            snprintf(display_name, sizeof(display_name), "#%s", channel->name);
        }
        gtk_tree_store_set(store, &iter, 0, display_name, 1, i, -1);
    }
    
    // Expand all
    gtk_tree_view_expand_all(GTK_TREE_VIEW(client.channel_list));
    
    // Add selection handler
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(client.channel_list));
    g_signal_connect(selection, "changed", G_CALLBACK(on_channel_selection_changed), NULL);
    
    // Set up column if not already done
    if (gtk_tree_view_get_n_columns(GTK_TREE_VIEW(client.channel_list)) == 0) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Channels", renderer, "text", 0, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(client.channel_list), column);
    }
}

void on_channel_selection_changed(GtkTreeSelection *selection, gpointer data) {
    (void)data;
    
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        int channel_idx;
        gtk_tree_model_get(model, &iter, 1, &channel_idx, -1);
        
        if (channel_idx >= 0 && client.active_server >= 0) {
            switch_to_channel(client.active_server, channel_idx);
        }
    }
}

void switch_to_channel(int server_idx, int channel_idx) {
    if (server_idx < 0 || server_idx >= client.server_count ||
        channel_idx < 0 || channel_idx >= client.servers[server_idx].channel_count) {
        return;
    }
    
    server_info_t *server = &client.servers[server_idx];
    channel_info_t *channel = &server->channels[channel_idx];
    
    server->active_channel = channel_idx;
    
    // Update chat area with channel's buffer
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(client.chat_area), channel->buffer);
    
    // Scroll to bottom
    GtkTextMark *mark = gtk_text_buffer_get_insert(channel->buffer);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(client.chat_area), mark);
    
    // Update window title
    char title[256];
    if (channel->is_private_msg) {
        snprintf(title, sizeof(title), "IRC Client - %s (@%s)", 
                server->name, channel->target_nick);
    } else {
        snprintf(title, sizeof(title), "IRC Client - %s (#%s)", 
                server->name, channel->name);
    }
    gtk_window_set_title(GTK_WINDOW(client.window), title);
    
    // Update status
    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg), "Switched to %s%s", 
            channel->is_private_msg ? "@" : "#", 
            channel->is_private_msg ? channel->target_nick : channel->name);
    update_status(status_msg);
    
    // Focus message entry
    gtk_widget_grab_focus(client.message_entry);
}

void append_message_to_channel(int server_idx, int channel_idx, const char *message) {
    if (server_idx < 0 || server_idx >= client.server_count ||
        channel_idx < 0 || channel_idx >= client.servers[server_idx].channel_count) {
        return;
    }
    
    channel_info_t *channel = &client.servers[server_idx].channels[channel_idx];
    
    // Append to buffer
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(channel->buffer, &iter);
    gtk_text_buffer_insert(channel->buffer, &iter, message, -1);
    
    // Auto-scroll if this is the active channel
    if (server_idx == client.active_server && 
        channel_idx == client.servers[server_idx].active_channel) {
        
        GtkTextMark *mark = gtk_text_buffer_get_insert(channel->buffer);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(client.chat_area), mark);
    }
}
