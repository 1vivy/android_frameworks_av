#include "CameraServiceExtFactory.h"
#include "CameraService.h"
#include <dlfcn.h>
#include <log/log.h>

namespace android {

void* CameraServiceExtFactory::sFunctionTable = nullptr;
void* CameraServiceExtFactory::sExtObject = nullptr;
int (*CameraServiceExtFactory::sOnTransactFunc)(void*, uint32_t, const Parcel&, Parcel*, uint32_t) = nullptr;
void (*CameraServiceExtFactory::sSetCameraServiceInstanceFunc)(void*, sp<CameraService>) = nullptr;
void* CameraServiceExtFactory::sGetExtOpModeFn = nullptr;
void* CameraServiceExtFactory::sBeforeConfigFn = nullptr;

void CameraServiceExtFactory::ensureLoaded() {
    if (sFunctionTable != nullptr) return;

    const char* libPath = "system_ext/lib64/libcsextimpl.so";
    void* handle = dlopen(libPath, RTLD_NOW);
    if (handle == nullptr) {
        ALOGE("CameraServiceExtFactory: dlopen failed: %s", dlerror());
        return;
    }
    ALOGI("CameraServiceExtFactory: dlopen succeeded, handle=%p", handle);

    typedef void* (*GetFactoryFunc)();
    GetFactoryFunc getExtFactoryImpl = (GetFactoryFunc)dlsym(handle, "getExtFactoryImpl");
    if (getExtFactoryImpl == nullptr) {
        ALOGE("CameraServiceExtFactory: dlsym getExtFactoryImpl failed: %s", dlerror());
        dlclose(handle);
        return;
    }
    ALOGI("CameraServiceExtFactory: getExtFactoryImpl at %p", getExtFactoryImpl);

    // Triple indirection as determined from logs: getExtFactoryImpl returns ptr to ptr to ptr to function
    void* ptrToPtr = getExtFactoryImpl();
    if (ptrToPtr == nullptr) {
        ALOGE("CameraServiceExtFactory: getExtFactoryImpl returned null");
        dlclose(handle);
        return;
    }

    void* ptrToFunc = *(void**)ptrToPtr;
    if (ptrToFunc == nullptr) {
        ALOGE("CameraServiceExtFactory: first deref gave null");
        dlclose(handle);
        return;
    }

    void* actualFunc = *(void**)ptrToFunc;
    if (actualFunc == nullptr) {
        ALOGE("CameraServiceExtFactory: second deref gave null");
        dlclose(handle);
        return;
    }
    ALOGI("CameraServiceExtFactory: actual factory function at %p", actualFunc);

    sFunctionTable = operator new(8);
    *(void**)sFunctionTable = actualFunc;
    ALOGI("CameraServiceExtFactory: function table at %p", sFunctionTable);

    // Resolve onTransact (for direct call via vtable)
    sOnTransactFunc = (int (*)(void*, uint32_t, const Parcel&, Parcel*, uint32_t))
        dlsym(handle, "_ZN7android20CameraServiceExtImpl10onTransactEjRKNS_6ParcelEPS1_j");
    if (sOnTransactFunc == nullptr) {
        ALOGE("CameraServiceExtFactory: dlsym onTransact failed: %s", dlerror());
    } else {
        ALOGI("CameraServiceExtFactory: onTransact found at %p", sOnTransactFunc);
    }

    sSetCameraServiceInstanceFunc = (void (*)(void*, sp<CameraService>))
        dlsym(handle, "_ZN7android20CameraServiceExtImpl24setCameraServiceInstanceENS_2spINS_13CameraServiceEEE");
    if (sSetCameraServiceInstanceFunc == nullptr) {
        ALOGE("CameraServiceExtFactory: dlsym setCameraServiceInstance failed: %s", dlerror());
    } else {
        ALOGI("CameraServiceExtFactory: setCameraServiceInstance found at %p",
                sSetCameraServiceInstanceFunc);
    }

    // R4 Depth-2 configure hooks (resolved raw; the caller casts to the typed signature).
    sGetExtOpModeFn = dlsym(handle,
            "_ZN7android20CameraServiceExtImpl25getExtensionOperatingModeERKNS_14CameraMetadataEmi");
    sBeforeConfigFn = dlsym(handle,
            "_ZN7android20CameraServiceExtImpl28beforeConfigureStreamsLockedERKNS_14CameraMetadataEmNS_7String8ERNS_7camera39StreamSetEi");
    ALOGI("CameraServiceExtFactory: getExtensionOperatingMode=%p beforeConfigureStreamsLocked=%p",
            sGetExtOpModeFn, sBeforeConfigFn);
}

void* CameraServiceExtFactory::getInstance() {
    ensureLoaded();
    return sFunctionTable;   // may be null
}

void* CameraServiceExtFactory::getExtObject() {
    ensureLoaded();
    if (sExtObject == nullptr) {
        if (sFunctionTable == nullptr) {
            ALOGE("CameraServiceExtFactory::getExtObject: extension not loaded");
            return nullptr;
        }
        void* actualFunc = *(void**)sFunctionTable;
        if (actualFunc == nullptr) return nullptr;
        typedef void* (*GetObjectFunc)();
        sExtObject = ((GetObjectFunc)actualFunc)();
        if (sExtObject == nullptr) {
            ALOGE("CameraServiceExtFactory: factory returned null");
            return nullptr;
        }
        ALOGI("CameraServiceExtFactory: real extension object at %p", sExtObject);
    }
    return sExtObject;
}

int CameraServiceExtFactory::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    void* extObject = getExtObject();
    if (extObject == nullptr) {
        return -1;
    }
    if (sOnTransactFunc == nullptr) {
        ALOGE("CameraServiceExtFactory::onTransact: no function pointer");
        return -1;
    }
    return sOnTransactFunc(extObject, code, data, reply, flags);
}

void CameraServiceExtFactory::setCameraServiceInstance(const sp<CameraService>& service) {
    void* extObject = getExtObject();
    if (extObject == nullptr || sSetCameraServiceInstanceFunc == nullptr) {
        ALOGE("CameraServiceExtFactory::setCameraServiceInstance: extension not ready");
        return;
    }
    sSetCameraServiceInstanceFunc(extObject, service);
}

bool CameraServiceExtFactory::isLoaded() {
    ensureLoaded();
    return sFunctionTable != nullptr;
}

void* CameraServiceExtFactory::extObject() {
    return getExtObject();
}

void* CameraServiceExtFactory::getExtensionOperatingModeFn() {
    ensureLoaded();
    return sGetExtOpModeFn;
}

void* CameraServiceExtFactory::beforeConfigureStreamsLockedFn() {
    ensureLoaded();
    return sBeforeConfigFn;
}

CameraServiceExtFactory::~CameraServiceExtFactory() {
    // No cleanup needed – the extension library manages its own singleton.
    ALOGV("CameraServiceExtFactory destructor (stub)");
}

} // namespace android
