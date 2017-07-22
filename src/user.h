#ifndef _USER_H
#define _USER_H

#include "enet/enet.h"
#include "common.h"

// TODO: Con/Destructors

struct UserData; // TODO: We forward-declare this here so we can use it for constructors of packets, probably not ideal

typedef uint8_t UserIdentifier;

// Client -> Server
// Sent as a first message when/after connecting to the server
// Tells to the server that a new user would like to join, along with that user's details
struct NetworkUserSetupPacket
{
    UserIdentifier userID;
    uint8 nameLength;
    char name[MAX_USER_NAME_LENGTH];

    template<typename Packet> bool serialize(Packet& packet);
};

// Server -> Client
// Sent any time a new user connects
// Tells the client what the details of the new user are, so they can communicate
struct NetworkUserConnectPacket
{
    UserIdentifier userID;
    ENetAddress address;

    uint8 nameLength;
    char name[MAX_USER_NAME_LENGTH];
    // TODO: assert(user.nameLength < MAX_USER_NAME_LENGTH);

    void populate(UserData& user);

    template<typename Packet> bool serialize(Packet& packet);
};

// Client -> Server
// Sent in response to the SETUP packet on connecting
// Tells the new client about all the existing clients (each of which receive a CONNECT packet)
struct NetworkUserInitPacket
{
    uint8 userCount;
    NetworkUserConnectPacket existingUsers[MAX_USERS];

    template<typename Packet> bool serialize(Packet& packet);
};

struct UserData
{
    UserIdentifier ID;
    int nameLength;
    char name[MAX_USER_NAME_LENGTH+1];

    ENetPeer* netPeer;
};

struct ServerUserData : UserData
{
    explicit ServerUserData(NetworkUserSetupPacket& setupPacket);
};

#endif // _user_H
