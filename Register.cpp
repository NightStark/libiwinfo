#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <log/log.h>

#include "jni.h"
#include <nativehelper/JNIHelp.h>

extern "C" {

extern void register_fengmi_net_iwinfo(JNIEnv* env);
//Vjextern JNINativeMethod gMethods[];

}


/*
static const char *classPathName = "com/android/commands/geth/Wifi";
void register_fengmi_net_iwinfo(JNIEnv* env) {
  //jniRegisterNativeMethods(env, "fengmi/net/IwInfo", gMethods, NELEM(gMethods));
  jclass clazz;
  clazz = env->FindClass(classPathName);
  if (clazz == NULL) {
      printf("FindClass[%s] failed.\n", classPathName);
      return;
  }

  if (env->RegisterNatives(clazz, gMethods, 1) < 0) {
      printf("RegisterNatives failed.\n");
      return;
  }

  return;
}
*/

jint JNI_OnLoad(JavaVM* vm, void*)
{
    JNIEnv* env;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        ALOGE("JavaVM::GetEnv() failed");
        abort();
    }

    assert(env != NULL);

    register_fengmi_net_iwinfo(env);

    return JNI_VERSION_1_6;
}
