# Install dependencies

## json-c
```
wget https://s3.amazonaws.com/json-c_releases/releases/json-c-0.17-nodoc.tar.gz
tar -xf ./json-c-0.17-nodoc.tar.gz
mkdir json-c-0.17/build
cd json-c-0.17/build
```
### Linux configure
```
cmake ..
```
### MAC OS X configure
```
cmake -DCMAKE_INSTALL_RPATH=/usr/local/lib ..
```
### Build
```
cmake --build .
```
### Linux install
```
sudo cmake --install .
sudo ldconfig
```
### MAC OS X install
```
cmake --install .
```

## libubox
```
git clone git://git.openwrt.org/project/libubox.git
mkdir libubox/build
cd libubox/build
```
### Linux configure
```
cmake -DBUILD_LUA:BOOL=OFF ..
```
### MAC OS X configure
```
cmake -DBUILD_LUA:BOOL=OFF -DCMAKE_INSTALL_RPATH=/usr/local/lib ..
```
### Build
```
cmake --build .
```
### Linux install
```
sudo cmake --install .
sudo ldconfig
```
### MAC OS X install
```
cmake --install .
```

## uci
```
git clone git://git.openwrt.org/project/uci.git
mkdir uci/build
cd uci/build
```
### Linux configure
```
cmake -DBUILD_LUA:BOOL=OFF ..
```
### MAC OS X configure
```
cmake -DBUILD_LUA:BOOL=OFF -DCMAKE_INSTALL_RPATH=/usr/local/lib -DCMAKE_MACOSX_RPATH=TRUE ..
```
### Build
```
cmake --build .
```
### Linux install
```
sudo cmake --install .
sudo ldconfig
```
### MAC OS X install
```
cmake --install .
```

# Build bacnet-stack
```
git clone https://github.com/stargieg/bacnet-stack-upstream.git
cd bacnet-stack-upstream
git checkout server-uci
```

## Linux build
```
BACNET_PORT=linux CSTANDARD=" -std=gnu17" UCI=1 UCI_LIB_DIR="/usr/local/lib" make -C apps/server-uci all
```

## MAC OS X build
```
BACNET_PORT=bsd CSTANDARD=" -std=gnu17" UCI=1 UCI_LIB_DIR="/usr/local/lib" make -C apps/server-uci all
```

## uci config files

### /etc/config/bacnet_dev
```
config dev '0'
        option Modelname 'Openwrt Router'
        option Location 'Europe'
        option app_ver '12.09'
        option Id '4715'
        option port '47808'
        option net '0'
        option bacdl 'bip'
        option debug '0'
        option broadcast_ff0e '65294'
        option broadcast_ff02 '65282'
        option Description 'Openwrt Routeräüöß'
        option Name 'worker2'
        option iface 'enp0s3'
        option enable '1'
```
### /etc/config/bacnet_ai
```
config ai 'default'
        option si_unit '98'
        option description 'Analog Input'
        option nc '1'
        option event '7'
        option limit '3'
        option high_limit '40'
        option low_limit '0'
        option dead_limit '0'
        option cov_increment '0.1'
        option max_value '100'
        option min_value '0'

config ai '0'
        option pgroup 'R801'
        option name 'R801_RT_AI'
        option description 'Raumtemperatur'
        option addr '0'
        option tagname 'modbus-s1'
        option si_unit '62'
        option dead_limit '0.5'
        option cov_increment '0.2'
        option resolution '0.1'
        option value_time '1521885295'
        option Out_Of_Service '1'
        option value '23.3'
```

## uci cmd

```
uci show bacnet_dev
uci show bacnet_ai
```

## Run
```
bin/bacserv
```

## Section Device

|Name|Type|Required|Default|Description|
|----|----|--------|-------|-----------|
|enable|boolean|yes|1|Enabled
|debug|boolean|no|0|debug
|Id|number|yes|4712|ID
|Name|string|yes|SimpleServer|Name
|Description|string|no|Openwrt Router|Description
|Location|string|no|Europe|Location
|bacdl|string|yes|bip|Data link 'arcnet' 'Arcnet' 'bip' 'bip6' 'ethernet' 'mstp'
|iface|string|yes(*)|eth0|Device name 'eth0' required if bacdl is arcnet, bip, bip6 or ethernet
|serial|string|yes(*)|/dev/ttyUSB0|Serial Port required if bacdl is mstp
|port|number|yes(*)|47808|"IP Port" required if bacdl is bip or bip6
|broadcast|number|no|(none)|Broadcast addr 65294 for ff0e or 65282 for ff02 if bacdl is bip or bip6
|bbmd_addr|hostname|no|(none)|BBMD IP Adresse if bacdl is bip or bip6
|bbmd_port|number|no|47808|BBMD IP Port if bacdl is bip or bip6
|bdt_addr_1|host|no|(none)|BDT IP Adresse 1
|bdt_port_1|number|no|47808|BDT IP Port 1
|bdt_mask_1|ip4addr|no|(none)|Broadcast mask 192.168.1.255 if bacdl is bip
|bdt_addr_2|host|no|(none)|BDT IP Adresse 2
|bdt_port_2|number|no|47808|BDT IP Port 2
|bdt_mask_2|ip4addr|no|(none)|Broadcast mask 192.168.1.255 if bacdl is bip
|bdt_addr_3|host|no|(none)|BDT IP Adresse 3
|bdt_port_3|number|no|47808|BDT IP Port 3
|bdt_mask_3|ip4addr|no|(none)|Broadcast mask 192.168.1.255 if bacdl is bip
|mac|number|yes(*)|1|MAC for MSTP 0-128 if bacdl is mstp
|max_master|number|no|128|MAX Master for MSTP 0-128 if bacdl is mstp
|max_frames|number|no|1|MAX Frames for MSTP 0-128 if bacdl is mstp
|baud|number|yes(*)|38400|Datarate 9600 19200 38400 57600 115200 if bacdl is mstp
|parity_bit|string|yes(*)|N|Parity Bit N, O, E if bacdl is mstp
|data_bit|number|yes(*)|8|Data Bit 5, 6, 7, 8 if bacdl is mstp
|stop_bit|number|yes(*)|1|Stop Bit 1, 2 if bacdl is mstp
|apdu_timeout|number|no|0|APDU timeout in ms
|apdu_retries|number|no|3|APDU retries
|invoke_id|number|no|(none)|Invoke ID
|net|number|no|0|NET|Number 0 or 6661


## mstp sample
```
uci set bacnet_dev.0.iface='/dev/cu.usbserial-14320'
uci set bacnet_dev.0.baud='9600'
uci set bacnet_dev.0.mac='42'
uci commit bacnet_dev
bin/bacserv
```
