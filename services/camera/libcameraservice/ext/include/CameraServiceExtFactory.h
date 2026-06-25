#pragma once

#include <binder/Parcel.h>
#include <utils/StrongPointer.h>

namespace android {

// Forward declaration – we will not define this class
class CameraService;
class ICameraServiceExt;

class CameraServiceExtFactory {
public:
    // Returns a pointer to a function table (as required by OxygenOS)
    static void* getInstance();
    static int onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags);
    static void setCameraServiceInstance(const sp<CameraService>& service);
    virtual ~CameraServiceExtFactory();

private:
    static void ensureLoaded();
    static void* getExtObject();
    static void* sFunctionTable;   // pointer to function pointer
    static void* sExtObject;        // the real extension object (as void*)
    static int (*sOnTransactFunc)(void*, uint32_t, const Parcel&, Parcel*, uint32_t);
    static void (*sSetCameraServiceInstanceFunc)(void*, sp<CameraService>);
};

} // namespace android
