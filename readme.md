i wonder how this will turn out haha

---

### 1. **Application Flow**
- **Startup**: Load saved servers from JSON → Create GTK interface → Enter event loop  
- **Add Server**: GUI dialog → Validate input → Add to server array → Save config
- **Connect**: Select server → Spawn network thread → Send IRC handshake → Update GUI
- **Join Channel**: User types `/join #channel` → Send IRC JOIN → Create channel object → Update channel list
- **Send Message**: User types message → Route to current channel → Send PRIVMSG → Echo to chat
- **Receive Message**: Network thread gets data → Parse IRC message → Queue GUI update → Display in appropriate channel

### 2. **Multi-Threading Architecture**
- **Main Thread**: Handles all GTK events, user input, GUI updates
- **Network Threads**: One per connected server, handles IRC protocol, socket I/O
- **Thread Safety**: Network threads use `g_idle_add()` to queue GUI updates on main thread

### 3. **Data Management**
- **Global Client**: Contains all servers, active selections, GTK widgets
- **Per-Server**: Connection details, socket, channels, network thread  
- **Per-Channel**: Name, message buffer, DM target, auto-join setting
- **Message History**: Each channel has separate `GtkTextBuffer` for persistent history

### 4. **IRC Protocol Implementation**
- **Connection**: TCP socket → NICK/USER commands → Wait for 001 welcome
- **Commands**: Parse user input → Format IRC commands → Send to server
- **Messages**: Receive data → Parse IRC format → Route to correct channel → Update GUI
- **Keepalive**: Respond to PING with PONG to maintain connection

### 5. **Configuration Persistence**
- **JSON Format**: Servers array with connection details and channel lists
- **Auto-Save**: Configuration saved when servers added or settings changed
- **Auto-Load**: Restore servers and channels on application startup
- **Auto-Connect**: Optionally reconnect to servers that were connected previously

---

## Dependencies

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install build-essential libgtk-3-dev libjson-c-dev pkg-config
```

### Linux (Fedora/CentOS/RHEL)
```bash
sudo dnf install gcc gtk3-devel json-c-devel pkg-config
# or for older versions:
sudo yum install gcc gtk3-devel json-c-devel pkg-config
```

### Linux (Arch)
```bash
sudo pacman -S gcc gtk3 json-c pkg-config
```

### Windows (MinGW-w64)
1. Install MSYS2 from https://www.msys2.org/
2. Open MSYS2 terminal and run:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk3 mingw-w64-x86_64-json-c mingw-w64-x86_64-pkg-config
```

## Building

### Linux
```bash
# Check dependencies
make check-deps

# Debug build
make debug

# Release build
make release

# Install system-wide (optional)
sudo make install
```

### Windows (MinGW)
```bash
# From MSYS2 MinGW64 terminal
make check-deps
make release

# Create distributable package
make package
```

## Usage

### Running the Application
```bash
# Linux
./bin/irc_client

# Windows
./bin/irc_client.exe
```

### Adding Servers
1. Click "Add Server" button
2. Fill in server details:
   - **Server Name**: Display name (e.g., "Libera Chat")
   - **Hostname**: Server address (e.g., "irc.libera.chat")
   - **Port**: Usually 6667 for plaintext, 6697 for SSL
   - **Nickname**: Your desired nickname
   - **Real Name**: Your real name or description
   - **Password**: Server password (optional)
   - **Auto-connect**: Connect automatically on startup

### Connecting to Servers
1. Select a server from the server list
2. Click "Connect" button
3. Wait for connection to establish
4. Server status will be shown in the status bar

### Joining Channels
- Type `/join #channelname` in the message area
- Channels will appear in the channel list
- Click on channel names to switch between them

### Private Messages
- Type `/msg nickname message` to send a private message
- A DM channel will be created automatically
- DM channels appear under "Direct Messages" section

### IRC Commands
All standard IRC commands are supported:
- `/join #channel` - Join a channel
- `/part #channel` - Leave a channel  
- `/quit` - Disconnect from server
- `/msg nick message` - Send private message
- `/nick newnick` - Change nickname
- `/me action` - Send action message
- Raw IRC commands can be sent by prefixing with `/`

## Configuration

Settings are automatically saved to `irc_config.json` in the application directory. The configuration includes:
- Server definitions with connection details
- Channel lists and auto-join settings
- Window layout preferences
- Auto-connect settings


## Architecture

The application uses a multi-threaded architecture:

- **Main Thread**: GUI event handling and user interaction
- **Network Threads**: One per server for handling IRC protocol
- **GUI Update Queue**: Thread-safe message passing for UI updates

### Thread Safety
- GUI updates use `g_idle_add()` for thread-safe operations
- Network operations are isolated per server
- Configuration changes are mutex-protected

## Security Considerations

- Passwords are stored in plaintext in the configuration file
- No SSL/TLS support yet (planned for future versions)
- DCC file transfers are not implemented
- No certificate validation for SSL connections

## Known Limitations

- SSL/TLS connections not yet supported
- No SASL authentication
- No file transfer capabilities
- Windows packaging requires manual DLL collection
- No notification system integration
- No message logging to files

## Development Roadmap

- [ ] SSL/TLS connections
- [ ] SASL authentication  
- [ ] Desktop notifications
- [ ] Message logging
- [ ] Improved Windows installer
- [ ] Plugin system
- [ ] Custom themes
- [ ] Voice/video calls (future)
