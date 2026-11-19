#include <windows.h>
#include "windows_log.hpp"
#include "windows_save.hpp"
#include <cstdio>
#include <exception>

namespace sw2 {
namespace {
HANDLE logFile=INVALID_HANDLE_VALUE;
SRWLOCK logLock=SRWLOCK_INIT;
void writeLine(std::string_view message) noexcept {
    if (logFile==INVALID_HANDLE_VALUE) return;
    SYSTEMTIME time{}; GetLocalTime(&time);
    char stamp[48];
    const int length=std::snprintf(stamp,sizeof(stamp),"%04u-%02u-%02u %02u:%02u:%02u.%03u ",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
    DWORD written{};
    WriteFile(logFile,stamp,DWORD(length),&written,nullptr);
    WriteFile(logFile,message.data(),DWORD(message.size()),&written,nullptr);
    WriteFile(logFile,"\r\n",2,&written,nullptr);
    FlushFileBuffers(logFile);
}
LONG WINAPI crash(EXCEPTION_POINTERS* problem) {
    char details[160];
    std::snprintf(details,sizeof(details),"FATAL unhandled Windows exception code=0x%08lX address=%p thread=%lu",
        problem->ExceptionRecord->ExceptionCode,problem->ExceptionRecord->ExceptionAddress,GetCurrentThreadId());
    // Never wait on a lock that may have been held by the faulting thread.
    const bool locked=TryAcquireSRWLockExclusive(&logLock);
    writeLine(details);
    if (locked) ReleaseSRWLockExclusive(&logLock);
    return EXCEPTION_EXECUTE_HANDLER;
}
}
void logEvent(std::string_view event) noexcept {
    AcquireSRWLockExclusive(&logLock); writeLine(event); ReleaseSRWLockExclusive(&logLock);
}
void initializeLog(const std::filesystem::path& executableDirectory) {
    auto open=[](const std::filesystem::path& folder) {
        return CreateFileW((folder/L"lastest.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    };
    logFile=open(executableDirectory);
    if (logFile==INVALID_HANDLE_VALUE) {
        const auto fallback=defaultSaveDirectory();
        std::error_code error; std::filesystem::create_directories(fallback,error);
        logFile=open(fallback);
    }
    SetUnhandledExceptionFilter(crash);
    std::set_terminate([] {
        logEvent("FATAL std::terminate");
        if (auto problem=std::current_exception()) {
            try {std::rethrow_exception(problem);} catch(const std::exception& e) {logEvent(e.what());} catch(...) {}
        }
        TerminateProcess(GetCurrentProcess(),1);
    });
    char system[160];
    std::snprintf(system,sizeof(system),"START DeadWarrior 2 Windows x64 pid=%lu processors=%lu simulation_hz=30",
        GetCurrentProcessId(),GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    logEvent(system);
}
}
