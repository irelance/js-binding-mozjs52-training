#include <iostream>
#include <sstream>
#include <fstream>

#include <stdio.h>
#include <sys/stat.h>

#include "jsapi.h"
#include "js/Initialization.h"

/* Use the fastest available getc. */
#if defined(HAVE_GETC_UNLOCKED)
# define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
# define fast_getc _getc_nolock
#else
# define fast_getc getc
#endif

static bool
GetBuildId(JS::BuildIdCharVector* buildId)
{
    const char buildid[] = "cocos_xdr";
    bool ok = buildId->append(buildid, strlen(buildid));
    return ok;
}
static const JSClassOps global_classOps = {
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr,
    nullptr,
    nullptr, nullptr, nullptr, JS_GlobalObjectTraceHook
};
static const JSClass global_class = {
    "global",
    JSCLASS_GLOBAL_FLAGS,
    &global_classOps
};

bool ReadFile(JSContext* cx, const std::string &filePath, JS::TranscodeBuffer& buffer)
{
    FILE *fp = fopen(filePath.c_str(), "rb");
    if (!fp) {
        return false;
    }
    /* Get the complete length of the file, if possible. */
    struct stat st;
    int ok = fstat(fileno(fp), &st);
    if (ok != 0)
        return false;
    if (st.st_size > 0) {
        if (!buffer.reserve(st.st_size))
            return false;
    }
    for (;;) {
        int c = fast_getc(fp);
        if (c == EOF)
            break;
        if (!buffer.append(c))
            return false;
    }

    return true;
}

bool DecompileFile(const std::string &inputFilePath, const std::string &outputFilePath, JSContext *cx) {
    JS::CompileOptions op(cx);
    op.setUTF8(true);
    op.setSourceIsLazy(true);
    op.setFileAndLine(inputFilePath.c_str(), 1);

    std::string ofp = outputFilePath;
    std::cout << "Input file: " << inputFilePath << std::endl;

    std::cout << "Loading ..." << std::endl;

    JS::RootedScript script(cx);
    JS::TranscodeBuffer loadBuffer;
    if(!ReadFile(cx,inputFilePath,loadBuffer)){
        std::cout << "Loading fails!" << std::endl;
        return false;
    }
    JS::TranscodeResult decodeResult = JS::DecodeScript(cx, loadBuffer, &script);
    if (decodeResult != JS::TranscodeResult::TranscodeResult_Ok)
    {
        std::cout << "Decoding fails!" << std::endl;
        return false;
    }
    JSString* result = JS_DecompileScript(cx, script, "test", 0);
    printf("%s\n", JS_EncodeString(cx, result));

    return true;
}

int main(int argc, const char *argv[])
{
    if (!JS_Init())
    {
        return false;
    }

    JSContext *cx = JS_NewContext(JS::DefaultHeapMaxBytes);
    if (nullptr == cx)
    {
        return false;
    }

    JS_SetGCParameter(cx, JSGC_MAX_BYTES, 0xffffffff);
    JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_INCREMENTAL);
    JS_SetNativeStackQuota(cx, 500000);
    JS_SetFutexCanWait(cx);
    JS_SetDefaultLocale(cx, "UTF-8");

    if (!JS::InitSelfHostedCode(cx))
    {
        return false;
    }

    JS_BeginRequest(cx);

    JS::CompartmentOptions options;
    options.behaviors().setVersion(JSVERSION_LATEST);
    options.creationOptions().setSharedMemoryAndAtomicsEnabled(true);

    JS::ContextOptionsRef(cx)
        .setIon(true)
        .setBaseline(true)
        .setAsmJS(true)
        .setNativeRegExp(true);


    JS::RootedObject global(cx, JS_NewGlobalObject(cx, &global_class, nullptr, JS::DontFireOnNewGlobalHook, options));

    JSCompartment *oldCompartment = JS_EnterCompartment(cx, global);

    if (!JS_InitStandardClasses(cx, global)) {
        std::cout << "JS_InitStandardClasses failed! " << std::endl;
    }

    JS_FireOnNewGlobalObject(cx, global);

    JS::SetBuildIdOp(cx, GetBuildId);

    DecompileFile("./compile.jsc","./decompile.js",cx);

    if (cx) {
        JS_LeaveCompartment(cx, oldCompartment);
        JS_EndRequest(cx);
        JS_DestroyContext(cx);
        JS_ShutDown();
        cx = nullptr;
    }
}
