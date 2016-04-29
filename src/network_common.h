#ifndef _NETWORK_COMMON_H
#define _NETWORK_COMMON_H

const unsigned char NET_MAX_CLIENTS = 8;

const unsigned char NET_MSGTYPE_AUDIO = 0x01;
const unsigned char NET_MSGTYPE_VIDEO = 0x02;
const unsigned char NET_MSGTYPE_INIT_DATA = 0x03;
const unsigned char NET_MSGTYPE_CLIENT_CONNECT = 0x04;
const unsigned char NET_MSGTYPE_CLIENT_DISCONNECT = 0x05;

#endif
