#include "user.h"

#include <assert.h>

#include "audio.h"
#include "video.h"
#include "logging.h"
#include "network.h"


bool UserIdentifier::operator ==(const UserIdentifier& other)
{
    return this->value == other.value;
}

template<typename Packet>
bool UserIdentifier::serialize(Packet& packet)
{
    packet.serializeuint8(this->value);
    return true;
}
template bool UserIdentifier::serialize(NetworkInPacket& packet);
template bool UserIdentifier::serialize(NetworkOutPacket& packet);

template<typename Packet>
bool NetworkUserSetupPacket::serialize(Packet& packet)
{
    if(!userID.serialize(packet))
        return false;
    packet.serializeuint8(this->nameLength);
    packet.serializestring(this->name, MAX_USER_NAME_LENGTH);

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
    if(!this->userID.serialize(packet))
        return false;

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

template<typename Packet>
bool NetworkUserDisconnectPacket::serialize(Packet& packet)
{
    if(!this->userID.serialize(packet))
        return false;
    return true;
}
template bool NetworkUserDisconnectPacket::serialize(NetworkInPacket& packet);
template bool NetworkUserDisconnectPacket::serialize(NetworkOutPacket& packet);

ServerUserData::ServerUserData(NetworkUserSetupPacket& setupPacket)
{
    this->ID = setupPacket.userID;
    this->nameLength = setupPacket.nameLength;
    memcpy(this->name, setupPacket.name, setupPacket.nameLength);
    this->name[setupPacket.nameLength] = 0;
}
