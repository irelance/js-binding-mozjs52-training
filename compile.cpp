#include <iostream>
#include <sstream>
#include <fstream>

#include <stdio.h>

#include "jsapi.h"
#include "js/Initialization.h"

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

bool WriteFile(const std::string &filePath, void *data, uint32_t length) {
    FILE *file = fopen(filePath.c_str(), "wb");
    if (file) {
        size_t ret = fwrite(data, 1, length, file);
        fclose(file);
        if (ret == length) {
            return true;
        }
    }
    return false;
}
bool CompileFile(const std::string &inputFilePath, const std::string &outputFilePath, JSContext *cx) {
    JS::CompileOptions op(cx);
    op.setUTF8(true);
    op.setSourceIsLazy(true);
    op.setFileAndLine(inputFilePath.c_str(), 1);

    std::string ofp = outputFilePath;
    std::cout << "Input file: " << inputFilePath << std::endl;

    std::cout << "Compiling ..." << std::endl;

    JS::RootedScript script(cx);
    bool ok = JS::Compile(cx, op, inputFilePath.c_str(), &script);

    if (!ok) {
        std::cout << "Compiled " << inputFilePath << " fails!" << std::endl;
        return false;
    }
    std::cout << "Encoding ..." << std::endl;

    JS::TranscodeBuffer buffer;
    JS::TranscodeResult encodeResult = JS::EncodeScript(cx, buffer, script);

    if (encodeResult != JS::TranscodeResult::TranscodeResult_Ok)
    {
        std::cout << "Encoding fails!" << std::endl;
        return false;
    }
    uint32_t length = (uint32_t)buffer.length();
    if (WriteFile(ofp, buffer.extractRawBuffer(), length)) {
        std::cout << "Done! " << "Output file: " << ofp << std::endl;
    }

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

    CompileFile("./compile.js","./compile.jsc",cx);

    if (cx) {
        JS_LeaveCompartment(cx, oldCompartment);
        JS_EndRequest(cx);
        JS_DestroyContext(cx);
        JS_ShutDown();
        cx = nullptr;
    }
}
