#include <stdint.h>
#include <string.h> // For strerror

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include "logging.h"
#include "video.h"

// https://linuxtv.org/downloads/v4l-dvb-apis-new/index.html

struct ImageBuffer
{
    void* data;
    size_t size;
};
static ImageBuffer* buffers;
static int bufferCount;

uint8_t* currentImage;

static int deviceFile;


static int xioctl(int fileDescriptor, int request, void* data)
{
        // NOTE: We check if the ioctl was interrupted by a signal and retry if it was.
        //       As far as I can tell, I only need to do this if I'm specifying signal handlers?
        int result;
        do
        {
            result = ioctl(fileDescriptor, request, data);
        } while((result == -1) && (errno == EINTR));
        return result;
}

bool Video::Setup()
{
    currentImage = new uint8_t[320*240*3];
    // TODO: List devices
    return true;
}

void Video::Shutdown()
{
    delete[] currentImage;
}

bool Video::enableCamera(int deviceId)
{
    // TODO: Disable the camera.
    //       The windows API takes deviceId = 0 to mean disable the camera, so
    //       we'll need to rework things a little so that we're consistent for this.
    deviceId = 0;
    char deviceName[32];
    snprintf(deviceName, sizeof(deviceName), "/dev/video%d", deviceId);
    logInfo("Opening video device: %s\n", deviceName);

    struct stat st;
    if(stat(deviceName, &st) == -1)
    {
        logFail("Error %d: Unable to get status for device %s: %s\n", errno, deviceName, strerror(errno));
        return false;
    }

    if(!S_ISCHR(st.st_mode))
    {
        logFail("Device %s is not a character special device\n", deviceName);
        return false;
    }

    // TODO: So the arguments here are (path, flags, mode), but there's also a (path, flags)
    //       overload. Can't we just use that?
    deviceFile = open(deviceName, O_RDWR | O_NONBLOCK, 0);

    if(deviceFile == -1)
    {
        logFail("Error %d: Unable to open device %s: %s\n", errno, deviceName, strerror(errno));
        return false;
    }

    v4l2_capability deviceCapabilities = {};
    if(xioctl(deviceFile, VIDIOC_QUERYCAP, &deviceCapabilities) == -1)
    {
        logFail("Error %d: %s is not a valid V4L2 device: %s\n", errno, deviceName, strerror(errno));
        close(deviceFile); // TODO: I don't believe we care if this fails? Can it? How?
        return false;
    }

    if(!(deviceCapabilities.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        logFail("Device %s does not support video capture\n", deviceName);
        close(deviceFile);
        return false;
    }

    if(!(deviceCapabilities.capabilities & V4L2_CAP_STREAMING))
    {
        logFail("Device %s does not support streaming IO\n", deviceName);
        close(deviceFile);
        return false;
    }

    // TODO: Here we're just resetting the crop region of the device to the default (IE the full image).
    //       This just means that we're not cropping, but is this necessary?
    //       Why/when would it not already be default?
    v4l2_cropcap cropCapabilities = {};
    cropCapabilities.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(deviceFile, VIDIOC_CROPCAP, &cropCapabilities) == 0)
    {
        v4l2_crop crop = {};
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropCapabilities.defrect;
        xioctl(deviceFile, VIDIOC_S_CROP, &crop);
    }

    // TODO: Set the format?
    v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 320;
    fmt.fmt.pix.height = 240;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    if(xioctl(deviceFile, VIDIOC_S_FMT, &fmt) == -1)
    {
        logWarn("Error %d: Unable to set device format: %s\n", errno, strerror(errno));
    }

    if(xioctl(deviceFile, VIDIOC_G_FMT, &fmt) == -1)
    {
        logWarn("Error %d: Unable to get device format: %s\n", errno, strerror(errno));
    }
    logInfo("Device format: %dx%d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    if(fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24)
    {
        // NOTE: We get 1448695129 = 'YUYV' = V4L2_PIX_FMT_YUYV
        logWarn("Format mismatch, unable to set the desired format. Got %d\n",
                fmt.fmt.pix.pixelformat);
    }
    if(fmt.fmt.pix.field != V4L2_FIELD_INTERLACED)
    {
        // NOTE: We get 1 = V4L2_FIELD_NONE
        logWarn("Field mismatch, unable to set the desired field. Got %d\n",
                fmt.fmt.pix.field);
    }

    v4l2_requestbuffers requestBuffers = {};
    requestBuffers.count = 4; // TODO: Why on earth do we need 4 buffers? Is it one per channel? Or something?
    requestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    requestBuffers.memory = V4L2_MEMORY_MMAP;
    if(xioctl(deviceFile, VIDIOC_REQBUFS, &requestBuffers) == -1)
    {
        logFail("Error %d: Failed to request memory buffers on device %s: %s\n",
                errno, deviceName, strerror(errno));
        close(deviceFile);
        return false;
    }
    // TODO: Check that we got a sensible number of buffers from the device.

    bufferCount = requestBuffers.count;
    buffers = new ImageBuffer[requestBuffers.count];
    if(!buffers)
    {
        logFail("Unable to allocate image buffers, out of memory\n");
        close(deviceFile);
        return false;
    }

    for(int i=0; i<requestBuffers.count; i++)
    {
        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if(xioctl(deviceFile, VIDIOC_QUERYBUF, &buffer) == -1)
        {
            // TODO: The docs only list EINVAL as a possible error if index is out of bounds.
            //       Do we really need to check this?
            logFail("Error %d: Unable to query the details of buffer %d: %s\n", errno, i, strerror(errno));
            // TODO: Cleanup the buffers that we succeeded on up until now
            close(deviceFile);
            return false;
        }

        buffers[i].size = buffer.length;
        buffers[i].data = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, deviceFile, buffer.m.offset);

        if(buffers[i].data == MAP_FAILED)
        {
            logFail("Unable to map device memory to user memory for buffer %d\n", i);
            // TODO: Cleanup
            close(deviceFile);
            return false;
        }
    }

    for(int i=0; i<bufferCount; i++)
    {
        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if(xioctl(deviceFile, VIDIOC_QBUF, &buffer) == -1)
        {
            logFail("Error %d: Unable to queue video buffer %d: %s\n", errno, i, strerror(errno));
            // TODO: Cleanup all the buffers
            close(deviceFile);
            return false;
        }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(xioctl(deviceFile, VIDIOC_STREAMON, &type) == -1)
    {
        logFail("Error %d: Unable to begin streaming video input: %s\n", errno, strerror(errno));
        // TODO: Cleanup all the buffers
        close(deviceFile);
        return false;
    }

    logInfo("Video device: %s successfully opened.\n", deviceName);
    return true;
}

bool Video::checkForNewVideoFrame()
{
    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    if(xioctl(deviceFile, VIDIOC_DQBUF, &buffer) == -1)
    {
        if(errno != EAGAIN)
        {
            logWarn("Error %d: Failed to dequeue video buffer: %s\n", errno, strerror(errno));
        }
        else
        {
            logTerm("Video frame not ready!\n");
        }
        return false;
    }

    // TODO: We could maybe do something sneaky here, like re-queue the *previous* buffer,
    //       rather than this one, which gives us a single working/active buffer.
    logTerm("Received %d bytes from the device! seq=%d\n", buffer.bytesused, buffer.sequence);
    memcpy(currentImage, buffers[buffer.index].data, 320*240*3);

    if(xioctl(deviceFile, VIDIOC_QBUF, &buffer) == -1)
    {
        logWarn("Error %d: Failed to re-queue video buffer: %s\n", errno, strerror(errno));
        // TODO: Well....crap...something broke somewhere? We should probably stop the device?
    }

    return true;
}

uint8_t* Video::currentVideoFrame()
{
    return nullptr;
}

int Video::encodeRGBImage(int inputLength, uint8_t* inputBuffer,
                          int outputLength, uint8_t* outputBuffer)
{
    return 0;
}

int Video::decodeRGBImage(int inputLength, uint8_t* inputBuffer,
                          int outputLength, uint8_t* outputBuffer)
{
    return 0;
}

