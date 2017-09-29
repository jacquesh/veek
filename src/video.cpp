#include "network.h"
#include "video.h"

// https://www.reddit.com/r/programming/comments/4rljty/got_fed_up_with_skype_wrote_my_own_toy_video_chat?st=iql0rqn9&sh=7602e95d
// https://github.com/rygorous/kkapture
// https://github.com/ofTheo/videoInput
// https://github.com/jarikomppa/escapi
// https://github.com/roxlu/video_capture
// https://people.xiph.org/~tterribe/pubs/lca2012/auckland/intro_to_video1.pdf
// https://people.xiph.org/~jm/daala/revisiting/
//
// https://sidbala.com/h-264-is-magic/
// https://github.com/leandromoreira/digital_video_introduction
// https://blogs.gnome.org/rbultje/2016/12/13/overview-of-the-vp9-video-codec/

// Screen Capture:
// https://msdn.microsoft.com/en-us/library/windows/desktop/dd183402(v=vs.85).aspx
// https://stackoverflow.com/questions/3291167/how-can-i-take-a-screenshot-in-a-windows-application
// https://stackoverflow.com/questions/5069104/fastest-method-of-screen-capturing
// https://www.codeproject.com/Articles/20367/Screen-Capture-Simple-Win-Dialog-Based
// https://www.codeproject.com/Articles/5051/Various-methods-for-capturing-the-screen
// https://github.com/reterVision/win32-screencapture

int cameraDeviceCount;
char** cameraDeviceNames;

int cameraWidth = 320; // TODO: We probably also want these to be static
int cameraHeight = 240;

template<typename Packet>
bool Video::NetworkVideoPacket::serialize(Packet& packet)
{
    packet.serializeuint8(this->srcUser);
    packet.serializeuint8(this->index);
    packet.serializeuint16(this->imageWidth);
    packet.serializeuint16(this->imageHeight);
    packet.serializeuint16(this->encodedDataLength);
    packet.serializebytes(this->encodedData, this->encodedDataLength);

    return true;
}
template bool Video::NetworkVideoPacket::serialize(NetworkInPacket& packet);
template bool Video::NetworkVideoPacket::serialize(NetworkOutPacket& packet);

#if defined(_WIN32) && !defined(__unix__)
#include "video_win32.cpp"

#elif !defined(_WIN32) && defined(__unix__)
#include "video_unix.cpp"

#endif
