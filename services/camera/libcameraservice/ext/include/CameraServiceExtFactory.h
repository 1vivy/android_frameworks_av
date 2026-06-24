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

    // R4 Depth-2 lifecycle hooks. The factory stays type-light: it only LOADS + exposes the OEM ext
    // object and the raw resolved CameraServiceExtImpl member-fn pointers; the caller (Camera3Device,
    // which has CameraMetadata/String8/camera3::StreamSet in scope) casts them to the typed signature
    // and invokes them. isLoaded() is the OOS-faithful "ext-enabled" gate (mirrors OOS's vtable-valid
    // check) — there is deliberately NO auth gate here: the ext self-gates on the com.oplus.packageName
    // stamp in the session metadata + its onTransact auth state. See oem-ext-depth2-lifecycle-RE.md.
    static bool  isLoaded();                          // ext dlopen + factory resolve succeeded
    static void* extObject();                         // the CameraServiceExtImpl* (member-fn `this`)
    static void* getExtensionOperatingModeFn();       // int(*)(this, const CameraMetadata&, ulong, int)
    static void* beforeConfigureStreamsLockedFn();    // void(*)(this, const CameraMetadata&, ulong, String8, camera3::StreamSet&, int)

    virtual ~CameraServiceExtFactory();

private:
    static void ensureLoaded();
    static void* getExtObject();
    static void* sFunctionTable;   // pointer to function pointer
    static void* sExtObject;        // the real extension object (as void*)
    static int (*sOnTransactFunc)(void*, uint32_t, const Parcel&, Parcel*, uint32_t);
    static void (*sSetCameraServiceInstanceFunc)(void*, sp<CameraService>);
    static void* sGetExtOpModeFn;          // resolved CameraServiceExtImpl::getExtensionOperatingMode
    static void* sBeforeConfigFn;          // resolved CameraServiceExtImpl::beforeConfigureStreamsLocked
};

} // namespace android
