// The Android/SDL-Android symbols the engine imports, answered for a host build.
//
// These are the entry points the engine imports from libSDL2.so (Aspyr's fork of its Android
// backend) and liblog.so. Android_JNI_GetEnv() hands out a stub JNIEnv on every platform, the
// Android build included: every one of its ~230 functions is a harmless no-op returning zero
// (FindClass and the ID lookups return a dummy non-null handle, string getters return ""). Every
// Java-bridge call in AndroidJNI.cpp / MacMain / CSWGuiMainMenu / CSWGuiSaveLoad / AspyrCloud
// therefore does nothing, which on a desktop is the right answer ("show the window", "is a
// controller attached", cloud saves...). Set K2_JNI_TRACE=1 to log the class/method names looked
// up.
#include <stdint.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <jni.h>

namespace {

bool Trace()
{
    static int s = -1;
    if (s < 0) s = getenv("K2_JNI_TRACE") != 0;
    return s != 0;
}

int s_dummy;   // the one "object" every lookup returns
jobject Dummy() { return reinterpret_cast<jobject>(&s_dummy); }

// Every slot holds a function of exactly the slot's own type. Native ABIs would let one generic
// "return 0" function stand in for all of them, but WebAssembly checks the type of every indirect
// call and traps on a mismatch ("function signature mismatch"), so the types are generated from the
// slots themselves. Zero answers 0 / null / false / 0.0 / nothing; Handle answers the dummy handle.
template <typename F> struct Slot;
template <typename R, typename... A> struct Slot<R (*)(A...)> {
    static R Zero(A...) { return R(); }
    static R Handle(A...) { return reinterpret_cast<R>(&s_dummy); }
};
template <typename R, typename... A> struct Slot<R (*)(A..., ...)> {
    static R Zero(A..., ...) { return R(); }
};
// Call<Type>Method and Get<Type>Field slots whose answer comes from the method or field id, their
// third argument on every form (varargs, V and A alike).
template <typename F> struct ById;
template <typename R, typename X, typename Id, typename... Rest> struct ById<R (*)(JNIEnv*, X, Id, Rest...)> {
    template <R (*Answer)(const void*)> static R Fn(JNIEnv*, X, Id id, Rest...) { return Answer(id); }
};
template <typename R, typename X, typename Id> struct ById<R (*)(JNIEnv*, X, Id, ...)> {
    template <R (*Answer)(const void*)> static R Fn(JNIEnv*, X, Id id, ...) { return Answer(id); }
};

jclass FindClass(JNIEnv*, const char* name)
{
    if (Trace()) fprintf(stderr, "k2: JNI FindClass %s\n", name);
    return static_cast<jclass>(Dummy());
}
// Every method id is an interned record of its name, so a Call*Method can answer per method:
// K2_JNI_<method>=<int> sets what the Boolean/Int calls on it return. Default 0, except the
// host-identity answers below.
struct MethodRec { char name[64]; int64_t value; };
jmethodID GetMethodID(JNIEnv*, jclass, const char* name, const char* sig)
{
    if (Trace()) fprintf(stderr, "k2: JNI method %s%s\n", name, sig);
    static MethodRec s_recs[256];
    static int s_n;
    for (int i = 0; i < s_n; ++i)
        if (strcmp(s_recs[i].name, name) == 0) return reinterpret_cast<jmethodID>(&s_recs[i]);
    if (s_n == 256) return reinterpret_cast<jmethodID>(Dummy());
    MethodRec* r = &s_recs[s_n++];
    snprintf(r->name, sizeof r->name, "%s", name);
    char key[96];
    snprintf(key, sizeof key, "K2_JNI_%s", name);
    const char* v = getenv(key);
    // HasTouchScreen: the Android build always has one, and the main interface shows the touch HUD
    // (top-right help/options/camera/quicksave, bottom-right party/stealth/pause/talk) only when it
    // says so. Only the touch UI build answers 1; pc and xbox answer 0, as a desktop or a console
    // does, so those widgets stay hidden (Esc and the keyboard/pad reach the same screens).
    // K2_JNI_HasTouchScreen=1 brings them back for a pc run.
#ifdef K2_UI_TOUCH
    const int64_t nTouchScreen = 1;
#else
    const int64_t nTouchScreen = 0;
#endif
    int64_t nDefault = strcmp(name, "HasTouchScreen") == 0 ? nTouchScreen : 0;
    r->value = v ? strtol(v, 0, 0) : nDefault;
    return reinterpret_cast<jmethodID>(r);
}
int64_t RecValue(const void* m) { return m && m != &s_dummy ? static_cast<const MethodRec*>(m)->value : 0; }
jboolean AnswerBool(const void* m) { return RecValue(m) != 0; }
jint AnswerInt(const void* m) { return static_cast<jint>(RecValue(m)); }
// Fields the same way: K2_JNI_<field>=<number> is what Get(Static)FloatField answers for it
// (KOTOR 1's SDL_main reads DisplayMetrics.xdpi / ydpi for its tap tolerances); default 0.
struct FieldRec { char name[64]; float value; };
jfieldID GetFieldID(JNIEnv*, jclass, const char* name, const char*)
{
    if (Trace()) fprintf(stderr, "k2: JNI field %s\n", name);
    static FieldRec s_recs[64];
    static int s_n;
    for (int i = 0; i < s_n; ++i)
        if (strcmp(s_recs[i].name, name) == 0) return reinterpret_cast<jfieldID>(&s_recs[i]);
    if (s_n == 64) return reinterpret_cast<jfieldID>(Dummy());
    FieldRec* r = &s_recs[s_n++];
    snprintf(r->name, sizeof r->name, "%s", name);
    char key[96];
    snprintf(key, sizeof key, "K2_JNI_%s", name);
    const char* v = getenv(key);
    r->value = v ? strtof(v, 0) : 0.0f;
    return reinterpret_cast<jfieldID>(r);
}
jfloat AnswerFloat(const void* f) { return f && f != &s_dummy ? static_cast<const FieldRec*>(f)->value : 0.0f; }
const char* EmptyUTF(JNIEnv*, jstring, jboolean* isCopy)
{
    if (isCopy) *isCopy = 0;
    return "";
}

JNINativeInterface* MakeTable()
{
    static JNINativeInterface t;   // reserved0..3 stay null
    // Every function slot first answers zero, each with a function of its own type (Slot above)...
#define Z(name) t.name = &Slot<decltype(t.name)>::Zero;
    Z(GetVersion) Z(DefineClass) Z(FindClass) Z(FromReflectedMethod)
    Z(FromReflectedField) Z(ToReflectedMethod) Z(GetSuperclass) Z(IsAssignableFrom)
    Z(ToReflectedField) Z(Throw) Z(ThrowNew) Z(ExceptionOccurred)
    Z(ExceptionDescribe) Z(ExceptionClear) Z(FatalError) Z(PushLocalFrame)
    Z(PopLocalFrame) Z(NewGlobalRef) Z(DeleteGlobalRef) Z(DeleteLocalRef)
    Z(IsSameObject) Z(NewLocalRef) Z(EnsureLocalCapacity) Z(AllocObject)
    Z(NewObject) Z(NewObjectV) Z(NewObjectA) Z(GetObjectClass)
    Z(IsInstanceOf) Z(GetMethodID) Z(CallObjectMethod) Z(CallObjectMethodV)
    Z(CallObjectMethodA) Z(CallBooleanMethod) Z(CallBooleanMethodV) Z(CallBooleanMethodA)
    Z(CallByteMethod) Z(CallByteMethodV) Z(CallByteMethodA) Z(CallCharMethod)
    Z(CallCharMethodV) Z(CallCharMethodA) Z(CallShortMethod) Z(CallShortMethodV)
    Z(CallShortMethodA) Z(CallIntMethod) Z(CallIntMethodV) Z(CallIntMethodA)
    Z(CallLongMethod) Z(CallLongMethodV) Z(CallLongMethodA) Z(CallFloatMethod)
    Z(CallFloatMethodV) Z(CallFloatMethodA) Z(CallDoubleMethod) Z(CallDoubleMethodV)
    Z(CallDoubleMethodA) Z(CallVoidMethod) Z(CallVoidMethodV) Z(CallVoidMethodA)
    Z(CallNonvirtualObjectMethod) Z(CallNonvirtualObjectMethodV) Z(CallNonvirtualObjectMethodA) Z(CallNonvirtualBooleanMethod)
    Z(CallNonvirtualBooleanMethodV) Z(CallNonvirtualBooleanMethodA) Z(CallNonvirtualByteMethod) Z(CallNonvirtualByteMethodV)
    Z(CallNonvirtualByteMethodA) Z(CallNonvirtualCharMethod) Z(CallNonvirtualCharMethodV) Z(CallNonvirtualCharMethodA)
    Z(CallNonvirtualShortMethod) Z(CallNonvirtualShortMethodV) Z(CallNonvirtualShortMethodA) Z(CallNonvirtualIntMethod)
    Z(CallNonvirtualIntMethodV) Z(CallNonvirtualIntMethodA) Z(CallNonvirtualLongMethod) Z(CallNonvirtualLongMethodV)
    Z(CallNonvirtualLongMethodA) Z(CallNonvirtualFloatMethod) Z(CallNonvirtualFloatMethodV) Z(CallNonvirtualFloatMethodA)
    Z(CallNonvirtualDoubleMethod) Z(CallNonvirtualDoubleMethodV) Z(CallNonvirtualDoubleMethodA) Z(CallNonvirtualVoidMethod)
    Z(CallNonvirtualVoidMethodV) Z(CallNonvirtualVoidMethodA) Z(GetFieldID) Z(GetObjectField)
    Z(GetBooleanField) Z(GetByteField) Z(GetCharField) Z(GetShortField)
    Z(GetIntField) Z(GetLongField) Z(GetFloatField) Z(GetDoubleField)
    Z(SetObjectField) Z(SetBooleanField) Z(SetByteField) Z(SetCharField)
    Z(SetShortField) Z(SetIntField) Z(SetLongField) Z(SetFloatField)
    Z(SetDoubleField) Z(GetStaticMethodID) Z(CallStaticObjectMethod) Z(CallStaticObjectMethodV)
    Z(CallStaticObjectMethodA) Z(CallStaticBooleanMethod) Z(CallStaticBooleanMethodV) Z(CallStaticBooleanMethodA)
    Z(CallStaticByteMethod) Z(CallStaticByteMethodV) Z(CallStaticByteMethodA) Z(CallStaticCharMethod)
    Z(CallStaticCharMethodV) Z(CallStaticCharMethodA) Z(CallStaticShortMethod) Z(CallStaticShortMethodV)
    Z(CallStaticShortMethodA) Z(CallStaticIntMethod) Z(CallStaticIntMethodV) Z(CallStaticIntMethodA)
    Z(CallStaticLongMethod) Z(CallStaticLongMethodV) Z(CallStaticLongMethodA) Z(CallStaticFloatMethod)
    Z(CallStaticFloatMethodV) Z(CallStaticFloatMethodA) Z(CallStaticDoubleMethod) Z(CallStaticDoubleMethodV)
    Z(CallStaticDoubleMethodA) Z(CallStaticVoidMethod) Z(CallStaticVoidMethodV) Z(CallStaticVoidMethodA)
    Z(GetStaticFieldID) Z(GetStaticObjectField) Z(GetStaticBooleanField) Z(GetStaticByteField)
    Z(GetStaticCharField) Z(GetStaticShortField) Z(GetStaticIntField) Z(GetStaticLongField)
    Z(GetStaticFloatField) Z(GetStaticDoubleField) Z(SetStaticObjectField) Z(SetStaticBooleanField)
    Z(SetStaticByteField) Z(SetStaticCharField) Z(SetStaticShortField) Z(SetStaticIntField)
    Z(SetStaticLongField) Z(SetStaticFloatField) Z(SetStaticDoubleField) Z(NewString)
    Z(GetStringLength) Z(GetStringChars) Z(ReleaseStringChars) Z(NewStringUTF)
    Z(GetStringUTFLength) Z(GetStringUTFChars) Z(ReleaseStringUTFChars) Z(GetArrayLength)
    Z(NewObjectArray) Z(GetObjectArrayElement) Z(SetObjectArrayElement) Z(NewBooleanArray)
    Z(NewByteArray) Z(NewCharArray) Z(NewShortArray) Z(NewIntArray)
    Z(NewLongArray) Z(NewFloatArray) Z(NewDoubleArray) Z(GetBooleanArrayElements)
    Z(GetByteArrayElements) Z(GetCharArrayElements) Z(GetShortArrayElements) Z(GetIntArrayElements)
    Z(GetLongArrayElements) Z(GetFloatArrayElements) Z(GetDoubleArrayElements) Z(ReleaseBooleanArrayElements)
    Z(ReleaseByteArrayElements) Z(ReleaseCharArrayElements) Z(ReleaseShortArrayElements) Z(ReleaseIntArrayElements)
    Z(ReleaseLongArrayElements) Z(ReleaseFloatArrayElements) Z(ReleaseDoubleArrayElements) Z(GetBooleanArrayRegion)
    Z(GetByteArrayRegion) Z(GetCharArrayRegion) Z(GetShortArrayRegion) Z(GetIntArrayRegion)
    Z(GetLongArrayRegion) Z(GetFloatArrayRegion) Z(GetDoubleArrayRegion) Z(SetBooleanArrayRegion)
    Z(SetByteArrayRegion) Z(SetCharArrayRegion) Z(SetShortArrayRegion) Z(SetIntArrayRegion)
    Z(SetLongArrayRegion) Z(SetFloatArrayRegion) Z(SetDoubleArrayRegion) Z(RegisterNatives)
    Z(UnregisterNatives) Z(MonitorEnter) Z(MonitorExit) Z(GetJavaVM)
    Z(GetStringRegion) Z(GetStringUTFRegion) Z(GetPrimitiveArrayCritical) Z(ReleasePrimitiveArrayCritical)
    Z(GetStringCritical) Z(ReleaseStringCritical) Z(NewWeakGlobalRef) Z(DeleteWeakGlobalRef)
    Z(ExceptionCheck) Z(NewDirectByteBuffer) Z(GetDirectBufferAddress) Z(GetDirectBufferCapacity)
    Z(GetObjectRefType)
#undef Z
    // ...then the ones that answer something else.
    t.FindClass = FindClass;
    t.GetMethodID = GetMethodID;
    t.GetStaticMethodID = GetMethodID;
    t.GetFieldID = GetFieldID;
    t.GetStaticFieldID = GetFieldID;
    t.GetStringUTFChars = EmptyUTF;
#define H(name) t.name = &Slot<decltype(t.name)>::Handle;
    H(GetObjectClass) H(NewStringUTF) H(NewGlobalRef)
#undef H
#define A(name, answer) t.name = &ById<decltype(t.name)>::template Fn<answer>;
#define B(name) A(name, AnswerBool)
#define I(name) A(name, AnswerInt)
#define F(name) A(name, AnswerFloat)
    B(CallBooleanMethod) B(CallBooleanMethodV) B(CallBooleanMethodA) B(CallStaticBooleanMethod)
    B(CallStaticBooleanMethodV) B(CallStaticBooleanMethodA)
    I(CallIntMethod) I(CallIntMethodV) I(CallIntMethodA) I(CallStaticIntMethod)
    I(CallStaticIntMethodV) I(CallStaticIntMethodA)
    F(GetFloatField) F(GetStaticFloatField)
#undef B
#undef I
#undef F
#undef A
    // A slot jni.h gains later and the list above misses would be a null call: fail here instead.
    void* const* slots = reinterpret_cast<void* const*>(&t);  // raw-offset: checks the table slot by slot
    for (size_t i = 4; i < sizeof t / sizeof(void*); ++i)
        if (!slots[i]) { fprintf(stderr, "k2: JNI stub table: slot %zu has no function\n", i); abort(); }
    return &t;
}

} // namespace

extern "C" JNIEnv* Android_JNI_GetEnv()
{
    static JNIEnv s_env = { MakeTable() };
    return &s_env;
}

#ifndef __ANDROID__
const char* k2shim_user_dir();

extern "C" const char* SDL_AndroidGetExternalStoragePath()
{
    // The OBB/save root on Android. Default to the working directory; K2_DATA_DIR overrides. With a
    // user directory the working directory is used too: the engine's paths under it stay relative,
    // which is what the shim's user layer reads and writes through.
    const char* p = getenv("K2_DATA_DIR");
    return p && !*k2shim_user_dir() ? p : ".";
}

#endif

// Aspyr's SDL-Android publishes the GL surface size here (stock SDL2 has no such globals); the engine
// reads it for its aspect decisions. The platform layer overwrites both once the window exists.
int g_SDL_BufferGeometry_w = 1280;
int g_SDL_BufferGeometry_h = 720;

#ifndef __ANDROID__
extern "C" int __android_log_write(int nPrio, const char* pszTag, const char* pszText)
{
    return fprintf(stderr, "[%d %s] %s\n", nPrio, pszTag ? pszTag : "", pszText ? pszText : "");
}

extern "C" int __android_log_print(int nPrio, const char* pszTag, const char* pszFmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, pszFmt);
    vsnprintf(buf, sizeof buf, pszFmt, ap);
    va_end(ap);
    return __android_log_write(nPrio, pszTag, buf);
}
#endif
