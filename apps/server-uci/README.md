# Install dependencies

## json-c
```
wget https://s3.amazonaws.com/json-c_releases/releases/json-c-0.18-nodoc.tar.gz
tar -xf ./json-c-0.18-nodoc.tar.gz
mkdir json-c-0.18/build
cd json-c-0.18/build
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
git clone https://github.com/openwrt/libubox.git
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
git clone https://github.com/openwrt/uci.git
git clone git://git.openwrt.org/project/libubox.git
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

## configure
```
mkdir -p build && cd build
cmake 	-DBACNET_STACK_BUILD_APPS:BOOL=OFF \
	-DBACDL_ARCNET:BOOL=ON \
	-DBACDL_BIP:BOOL=ON \
	-DBACDL_BIP6:BOOL=ON \
	-DBACDL_BSC:BOOL=OFF \
	-DBACDL_ETHERNET:BOOL=ON \
	-DBACDL_MSTP:BOOL=ON \
	-DBAC_ROUTING:BOOL=OFF \
	-DINTRINSIC_REPORTING:BOOL=ON \
	-DBACNET_SEGMENTATION_ENABLED:BOOL=ON \
	-DBACNET_BACKUP_RESTORE:BOOL=OFF \
	-DBACNET_PROPERTY_LISTS:BOOL=OFF \
	-DBACNET_BUILD_SERVER_MINI_APP:BOOL=OFF \
	-DBACNET_BUILD_SERVER_BASIC_APP:BOOL=OFF \
	-DBACNET_BUILD_BACPOLL_APP:BOOL=OFF \
	-DBACNET_BUILD_BACDISCOVER_APP:BOOL=OFF \
	-DUCI:BOOL=ON \
        -DUCI_CONFDIR:STRING=/usr/local/etc/bacnet \
	-DSERVER_UCI:BOOL=ON \
	-DBACNET_PROTOCOL_REVISION=24 \
	-DBACNET_STACK_DEPRECATED_DISABLE:BOOL=ON \
	-DCMAKE_BUILD_TYPE="Release" \
	-DCMAKE_TARGET="server-uci" \
        ..
```
## compile

```
cmake --build .
```
or more specific
```
cmake --build . --config Debug --target all --
cmake --build . --config Release --target server-uci --
```

## uci config files

see example files in apps/server-uci/config

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
|bacdl|string|yes|bip|Data link arcnet bip bip6 ethernet mstp
|iface|string|yes(*)|eth0|Device name eth0 or /dev/ttyUSB0 for Serial an Port
|port|number|yes(*)|47808|IP Port required if bacdl is bip or bip6
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
|mac_address|number|yes(*)|1|MAC for MSTP 0-128 if bacdl is mstp
|max_master|number|no|128|MAX Master for MSTP 0-128 if bacdl is mstp
|max_info_frames|number|no|1|MAX Info Frames for MSTP 0-128 if bacdl is mstp
|baud_rate|number|yes(*)|38400|Datarate 9600 19200 38400 57600 115200 if bacdl is mstp
|parity_bit|string|yes(*)|N|Parity Bit N, O or E if bacdl is mstp
|data_bit|number|yes(*)|8|Data Bit 5, 6, 7 or 8 if bacdl is mstp
|stop_bit|number|yes(*)|1|Stop Bit 1 or 2 if bacdl is mstp
|apdu_timeout|number|no|3000ms(*)|APDU timeout in ms if bacdl is mstp fix timout 60000ms
|apdu_retries|number|no|3|APDU retries
|invoke_id|number|no|(none)|Invoke ID
|net|number|no|0|NET|Number eg 0 for bip or mstp 6661


## mstp sample
```
uci set bacnet_dev.0.iface='/dev/cu.usbserial-14320'
uci set bacnet_dev.0.baud='9600'
uci set bacnet_dev.0.mac='42'
uci commit bacnet_dev
bin/bacserv
```
