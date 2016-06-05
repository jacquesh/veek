#ifndef _NETWORK_COMMON_H
#define _NETWORK_COMMON_H

const unsigned char NET_MSGTYPE_AUDIO = 0x01;
const unsigned char NET_MSGTYPE_VIDEO = 0x02;
const unsigned char NET_MSGTYPE_INIT_DATA = 0x03;
const unsigned char NET_MSGTYPE_CLIENT_CONNECT = 0x04;
const unsigned char NET_MSGTYPE_CLIENT_DISCONNECT = 0x05;

const int NET_PORT = 12345;

enum NetConnectionState
{
    NET_CONNSTATE_DISCONNECTED,
    NET_CONNSTATE_CONNECTING,
    NET_CONNSTATE_CONNECTED
};

#endif
