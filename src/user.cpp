#include <assert.h>
#include <string.h>

#include "audio.h"
#include "logging.h"
#include "network.h"
#include "user.h"
#include "video.h"

bool RoomIdentifier::equals(RoomIdentifier& other)
{
    return strncmp(name, other.name, MAX_ROOM_ID_LENGTH) == 0;
}

template<typename Packet>
bool NetworkUserSetupPacket::serialize(Packet& packet)
{
    packet.serializeuint16(this->userID);
    packet.serializeuint8(this->nameLength);
    packet.serializestring(this->name, MAX_USER_NAME_LENGTH);
    packet.serializebool(this->createRoom);
    packet.serializestring(this->roomId.name, MAX_ROOM_ID_LENGTH);

    return true;
}
template bool NetworkUserSetupPacket::serialize(NetworkInPacket& packet);
template bool NetworkUserSetupPacket::serialize(NetworkOutPacket& packet);


void NetworkUserConnectPacket::populate(UserData& user)
{
    this->userID = user.ID;
    this->address = user.netPeer->address;
    this->nameLength = (uint8)user.nameLength;
    memcpy(this->name, user.name, user.nameLength);
}

template<typename Packet>
bool NetworkUserConnectPacket::serialize(Packet& packet)
{
    packet.serializeuint16(this->userID);
    packet.serializeuint32(this->address.host);
    packet.serializeuint16(this->address.port);

    packet.serializeuint8(this->nameLength);
    packet.serializestring(this->name, MAX_USER_NAME_LENGTH);
    return true;
}
template bool NetworkUserConnectPacket::serialize(NetworkInPacket& packet);
template bool NetworkUserConnectPacket::serialize(NetworkOutPacket& packet);

template<typename Packet>
bool NetworkUserInitPacket::serialize(Packet& packet)
{
    packet.serializestring(this->roomId.name, MAX_ROOM_ID_LENGTH);
    packet.serializeuint8(this->userCount);

    for(int i=0; i<this->userCount; i++)
    {
        if(!existingUsers[i].serialize(packet))
            return false;
    }
    return true;
}
template bool NetworkUserInitPacket::serialize(NetworkInPacket& packet);
template bool NetworkUserInitPacket::serialize(NetworkOutPacket& packet);

ServerUserData::ServerUserData(NetworkUserSetupPacket& setupPacket)
{
    this->ID = setupPacket.userID;
    this->nameLength = setupPacket.nameLength;
    memcpy(this->name, setupPacket.name, setupPacket.nameLength);
    this->name[setupPacket.nameLength] = 0;
}
