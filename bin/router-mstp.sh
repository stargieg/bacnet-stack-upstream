#!/bin/bash
echo Setting parameters for BACnet/IP to MSTP Router

# BACnet/IP configuration
BACNET_IFACE=${1}
export BACNET_IFACE
echo "BACNET_IFACE=$BACNET_IFACE"

BACNET_IP_PORT=47808
export BACNET_IP_PORT
echo "BACNET_IP_PORT=$BACNET_IP_PORT"

# BBMD port for local apps
BACNET_BBMD_PORT=47809
export BACNET_BBMD_PORT
echo "BACNET_BBMD_PORT=$BACNET_BBMD_PORT"

# BACnet MSTP settings
BACNET_MSTP_IFACE=${2}
export BACNET_MSTP_IFACE
echo "BACNET_MSTP_IFACE=$BACNET_MSTP_IFACE"

BACNET_MSTP_BAUD=38400
export BACNET_MSTP_BAUD
echo "BACNET_MSTP_BAUD=$BACNET_MSTP_BAUD"

BACNET_MSTP_MAC=1
export BACNET_MSTP_MAC
echo "BACNET_MSTP_MAC=$BACNET_MSTP_MAC"

BACNET_MAX_INFO_FRAMES=128
export BACNET_MAX_INFO_FRAMES
echo "BACNET_MAX_INFO_FRAMES=$BACNET_MAX_INFO_FRAMES"

BACNET_MAX_MASTER=127
export BACNET_MAX_MASTER
echo "BACNET_MAX_MASTER=$BACNET_MAX_MASTER"

# Network Numbers
BACNET_IP_NET=1
export BACNET_IP_NET
echo "BACNET_IP_NET=$BACNET_IP_NET"

BACNET_MSTP_NET=2
export BACNET_MSTP_NET
echo "BACNET_MSTP_NET=$BACNET_MSTP_NET"

BACNET_ROUTER_DEBUG=1
export BACNET_ROUTER_DEBUG
echo "BACNET_ROUTER_DEBUG=$BACNET_ROUTER_DEBUG"

echo Launching new shell using the BACnet Router environment...
/bin/bash