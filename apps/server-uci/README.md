# Dep

## libubox
```
cmake --help-policy CMP0042
git clone git://git.openwrt.org/project/libubox.git
mkdir libubox/build;cd libubox/build; cmake -D BUILD_LUA:BOOL=OFF -D BUILD_EXAMPLES:BOOL=OFF ..;cmake --build .
sudo cmake --install .
sudo ldconfig
```

## uci
```
git clone git://git.openwrt.org/project/uci.git
mkdir uci/build;cd uci/build; cmake -D BUILD_LUA:BOOL=OFF ..;cmake --build .
sudo cmake --install .
sudo ldconfig
```

## bacnet-stack
```
git clone https://github.com/stargieg/bacnet-stack-upstream.git
cd bacnet-stack-upstream ; git checkout server-uci
```

## Linux
```
BACNET_PORT=linux CSTANDARD=" -std=gnu17" UCI=1 UCI_LIB_DIR="/usr/local/lib" BACNET_SRC_DIR=../../src BACNET_PORT_DIR=../../ports/linux make -C apps/server-uci all
```

## MAC OS X
```
BACNET_PORT=bsd CSTANDARD=" -std=gnu17" UCI=1 UCI_LIB_DIR="/usr/local/lib" BACNET_SRC_DIR=../../src BACNET_PORT_DIR=../../ports/bsd make -C apps/server-uci all
```

## Run
```
bin/bacserv
```

### uci config files

#### /etc/config/bacnet_dev
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
#### /etc/config/bacnet_ai
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

### uci cmd

```
uci show bacnet_dev
uci show bacnet_ai
```
