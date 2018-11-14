# udprepeater
The service repeated udp packets to multiple servers based on dns SRV records.
As destination address can be used NS records type SRV. If type SRV record is not found then an type A record is searched.
Can be used as load balancer for one instance, because UDP packets sending to all found servers (no roundrobin).

#### Maintainer Dmitry Teikovtsev <teikovtsev.dmitry@pdffiller.team>

### Usage
```bash
udprepeater [path/to/config/file.conf]
```
if parameter [path/to/config/file.conf] exist then try this as path to config file, elese fild fonfig file in folder /etc like /etc/udprepeater.conf

### Make Ubuntu dependensies:
```bash
sudo apt install build-essential
```

### Ubuntu instalation:
```bash
make install
```

### docker environment:
```bash
# debug mode 0 - error only, 1 - debug, 2 - do not advise, default: 1
UDP_REPEATER_DEBUG_LEVEL

# bind addres for UDP proxy server, default: 127.0.0.1
UDP_REPEATER_BIND_ADDR

# bind port for UDP proxy server, default: 30000
UDP_REPEATER_BIND_PORT

# destination server addres should be NS records type SRV or A, default: udp.server.example.com
UDP_REPEATER_DESTINATION_SERVER

# destination port.
# used only for NS record type A. If type SRV port used from NS records, default: 30002
UDP_REPEATER_DESTINATION_PORT

# maximum time in seconds between resolving DNS records, default: 60
UDP_REPEATER_MAX_TTL
```

### Create docker container:
```bash
docker build -t udprepeater .
```

### Run docker container:
```bash
docker run -e "UDP_REPEATER_DESTINATION_SERVER=myudpserver.com" udpreapeter
```
