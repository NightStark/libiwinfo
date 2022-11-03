#include <stdlib.h>
#include <unistd.h>

#include <log/log.h>

#include "jni.h"
#include <nativehelper/JNIHelp.h>

extern "C" {

extern void register_Fengmi_net_iwinfo(JNIEnv* env);

}

jint JNI_OnLoad(JavaVM* vm, void*) { JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        ALOGE("JavaVM::GetEnv() failed");
        abort();
    }

    register_Fengmi_net_iwinfo(env);

    return JNI_VERSION_1_6;
}
