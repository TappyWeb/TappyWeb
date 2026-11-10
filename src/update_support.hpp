#pragma once
#include <filesystem>
#include <fstream>
#include <array>
#include <regex>
#include <vector>
#include <string>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace tappy_update {
namespace fs=std::filesystem;
inline fs::path utf8(const std::string& text){std::u8string value;for(unsigned char c:text)value+=char8_t(c);return fs::path(value);}
inline constexpr const char* version="0.3.0";
inline std::array<int,3> parts(std::string value) {
    if(!value.empty()&&value[0]=='v')value.erase(0,1);
    std::smatch match;if(!std::regex_match(value,match,std::regex("([0-9]{1,6})\\.([0-9]{1,6})\\.([0-9]{1,6})")))return {-1,-1,-1};
    return {std::stoi(match[1]),std::stoi(match[2]),std::stoi(match[3])};
}
inline fs::path cache_root() {
#ifdef _WIN32
    wchar_t* value=_wgetenv(L"LOCALAPPDATA");return value?fs::path(value)/L"Tappyweb"/L"Updates":fs::path{};
#else
    const char* value=std::getenv("HOME");
#ifdef __APPLE__
    return value?fs::path(value)/"Library/Application Support/Tappyweb/Updates":fs::path{};
#else
    return value?fs::path(value)/".cache/tappyweb/updates":fs::path{};
#endif
#endif
}
inline fs::path executable() {
#ifdef _WIN32
    std::wstring value(32768,L'\0');DWORD count=GetModuleFileNameW(nullptr,value.data(),DWORD(value.size()));value.resize(count);return value;
#else
    #ifdef __APPLE__
    uint32_t count=0;_NSGetExecutablePath(nullptr,&count);std::string value(count,'\0');if(_NSGetExecutablePath(value.data(),&count))return {};return fs::weakly_canonical(value.c_str());
    #else
    std::string value(32768,'\0');auto count=readlink("/proc/self/exe",value.data(),value.size());if(count<0)return {};value.resize(size_t(count));return value;
    #endif
#endif
}
#ifdef _WIN32
inline std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\"";size_t slashes=0;
    for(wchar_t c:value) { if(c==L'\\'){++slashes;continue;}if(c==L'\"'){result.append(slashes*2+1,L'\\');result+=c;}else{result.append(slashes,L'\\');result+=c;}slashes=0; }
    result.append(slashes*2,L'\\');return result+L"\"";
}
inline int process(const std::vector<fs::path>& args,bool wait=true,bool quiet=false) {
    std::wstring command;for(auto& arg:args){if(!command.empty())command+=L' ';command+=quote(arg.wstring());}
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(args[0].c_str(),command.data(),nullptr,nullptr,TRUE,quiet?CREATE_NO_WINDOW:0,nullptr,nullptr,&startup,&child))return -1;
    CloseHandle(child.hThread);int result=0;
    if(wait){WaitForSingleObject(child.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(child.hProcess,&code);result=int(code);}CloseHandle(child.hProcess);return result;
}
#else
inline int process(const std::vector<fs::path>& args,bool wait=true,bool quiet=false) {
    pid_t pid=fork();if(pid<0)return -1;
    if(pid==0){if(!wait){pid_t grandchild=fork();if(grandchild<0)_exit(127);if(grandchild>0)_exit(0);setsid();}
        if(quiet){int sink=open("/dev/null",O_RDWR);if(sink>=0){dup2(sink,0);dup2(sink,1);dup2(sink,2);if(sink>2)close(sink);}}
        std::vector<std::string> values;std::vector<char*> argv;for(auto& arg:args)values.push_back(arg.string());for(auto& value:values)argv.push_back(value.data());argv.push_back(nullptr);execv(argv[0],argv.data());_exit(127);}
    int status=0;while(waitpid(pid,&status,0)<0&&errno==EINTR){}return WIFEXITED(status)?WEXITSTATUS(status):1;
}
#endif
inline const char* runtime_name() {
#ifdef _WIN32
    return "tappy.exe";
#else
    return "tappy";
#endif
}
inline fs::path helper() {
#ifdef _WIN32
    return executable().parent_path()/"tappy-update.exe";
#else
    return executable().parent_path()/"tappy-update";
#endif
}
inline int forward(int argc,char** argv) {
    if(std::getenv("TAPPY_NO_UPDATE"))return -1;
    auto root=cache_root();if(root.empty())return -1;std::ifstream input(root/"current.txt");std::string latest;input>>latest;
    if(parts(latest)<=parts(version)||parts(latest)[0]<0)return -1;
    auto binary=root/latest/runtime_name();std::error_code error;if(!fs::is_regular_file(binary,error)||fs::equivalent(binary,executable(),error))return -1;
    std::vector<fs::path> args{binary};for(int i=1;i<argc;++i)args.emplace_back(utf8(argv[i]));
    return process(args);
}
inline void check_in_background() {
    if(std::getenv("TAPPY_NO_UPDATE"))return;
    auto binary=helper();std::error_code error;
    if(!fs::exists(executable().parent_path()/"update-enabled",error)||!fs::is_regular_file(binary,error))return;
    process({binary,"--background",version},false,true);
}
}
