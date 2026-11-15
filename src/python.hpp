#pragma once
#include "json.hpp"
#ifndef _WIN32
#include <signal.h>
#endif

// The bridge is embedded so installed runtimes need no companion scripts.
namespace tappy_python {
inline constexpr const char* bridge=R"PY(import contextlib
import json
import math
import os
import sys
import traceback

request_path, result_path = sys.argv[1:]
with open(request_path, encoding="utf-8") as stream:
    request = json.load(stream)
try:
    os.chdir(request["directory"])
    sys.path.insert(0, request["directory"])
    code = compile(request["code"], request["filename"], "exec")
    values = request["values"]
    if not request["check"]:
        scope = dict(values)
        scope.update(__name__="__main__", __file__=request["filename"])
        # Prints are ordinary server output, never part of the data protocol.
        with contextlib.redirect_stdout(sys.stderr):
            exec(code, scope)
        updated = {}
        for name in request["state"]:
            original = values[name]
            value = scope.get(name)
            valid = (type(value) is type(original) or
                     type(original) in (int, float) and type(value) in (int, float))
            if not valid:
                raise TypeError("Keep the same type for state '" + name + "'.")
            if type(value) in (int, float) and not math.isfinite(value):
                raise ValueError("State numbers must be finite.")
            if type(value) is list and (len(value) > 10000 or any(type(item) is not str for item in value)):
                raise TypeError("State lists contain at most 10,000 strings.")
            updated[name] = value
        values = updated
    result = {"values": values}
    # Serialize inside the try so unsupported/oversized results produce a useful error.
    payload = json.dumps(result, ensure_ascii=True, allow_nan=False)
    if len(payload) > 1024 * 1024:
        raise ValueError("Python state exceeds the 1 MiB result limit.")
except BaseException:
    payload = json.dumps({"error": traceback.format_exc()[-12000:]})
with open(result_path, "w", encoding="utf-8") as stream:
    stream.write(payload)
)PY";

inline int process(const vector<fs::path>& args) {
#ifdef _WIN32
    wstring command;for(auto& arg:args){if(!command.empty())command+=L' ';command+=tappy_update::quote(arg.wstring());}
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child))return -1;
    CloseHandle(child.hThread);
    DWORD status=WaitForSingleObject(child.hProcess,10000);
    if(status!=WAIT_OBJECT_0){TerminateProcess(child.hProcess,1);WaitForSingleObject(child.hProcess,INFINITE);CloseHandle(child.hProcess);return -2;}
    DWORD code=1;GetExitCodeProcess(child.hProcess,&code);CloseHandle(child.hProcess);return int(code);
#else
    // Prepare argv before fork: the server may have other threads.
    vector<string> values;vector<char*> argv;for(auto& arg:args)values.push_back(arg.string());for(auto& value:values)argv.push_back(value.data());argv.push_back(nullptr);
    pid_t pid=fork();if(pid<0)return -1;
    if(pid==0){execvp(argv[0],argv.data());_exit(127);}
    auto deadline=chrono::steady_clock::now()+chrono::seconds(10);int status=0;
    for(;;){auto result=waitpid(pid,&status,WNOHANG);if(result==pid)return WIFEXITED(status)?WEXITSTATUS(status):1;if(result<0&&errno!=EINTR)return -1;
        if(chrono::steady_clock::now()>=deadline){kill(pid,SIGKILL);while(waitpid(pid,&status,0)<0&&errno==EINTR){}return -2;}this_thread::sleep_for(chrono::milliseconds(10));}
#endif
}

inline fs::path interpreter(const fs::path& directory) {
#ifdef _WIN32
    if(auto configured=_wgetenv(L"TAPPY_PYTHON");configured&&*configured)return fs::path(configured);
    auto local=directory/".venv/Scripts/python.exe";
    if(fs::is_regular_file(local))return local;
    if(auto active=_wgetenv(L"VIRTUAL_ENV");active&&*active){auto path=fs::path(active)/"Scripts/python.exe";if(fs::is_regular_file(path))return path;}
    return "python.exe";
#else
    if(auto configured=getenv("TAPPY_PYTHON");configured&&*configured)return utf8Path(configured);
    auto local=directory/".venv/bin/python";
    if(fs::is_regular_file(local))return local;
    if(auto active=getenv("VIRTUAL_ENV");active&&*active){auto path=fs::path(active)/"bin/python";if(fs::is_regular_file(path))return path;}
    return "python3";
#endif
}

inline Env run(const Node& node,const Env& values,const Env& state,bool check) {
    if(node.t.size()>2||(node.t.size()==2&&(!node.t[1].quoted||!node.python.empty()))||!node.children.empty())fail(node.line,"Use python: with indented code, or python \"script.py\".");
    fs::path source=utf8Path(filename),directory=source.parent_path();
    string code=node.python,codeFile=filename;
    if(node.t.size()==2){auto path=directory/utf8Path(node.t[1].text);codeFile=pathText(path);code=readFile(path);}
    else {if(trim(code).empty())fail(node.line,"Add indented Python code inside python:.");code=string(size_t(node.line),'\n')+code;}
    nlohmann::json request={{"directory",pathText(directory)},{"filename",codeFile},{"code",code},{"check",check},{"values",nlohmann::json::object()},{"state",nlohmann::json::array()}};
    for(auto& [name,value]:values)visit([&](const auto& item){request["values"][name]=item;},value);
    for(auto& [name,value]:state){(void)value;request["state"].push_back(name);}
    auto temp=fs::temp_directory_path()/ ("tappy-python-"+token());
    if(!fs::create_directory(temp))fail(node.line,"Cannot create Python workspace.");
#ifndef _WIN32
    fs::permissions(temp,fs::perms::owner_all,fs::perm_options::replace);
#endif
    struct Cleanup {fs::path path;~Cleanup(){error_code ec;fs::remove_all(path,ec);}} cleanup{temp};
    writeFile(temp/"bridge.py",bridge);writeFile(temp/"request.json",request.dump());
    int status=process({interpreter(directory),temp/"bridge.py",temp/"request.json",temp/"result.json"});
    if(status==-2)fail(node.line,"Python exceeded the 10 second time limit.");
    if(status!=0||!fs::exists(temp/"result.json"))fail(node.line,"Cannot run Python 3. Install Python or set TAPPY_PYTHON to its executable path.");
    if(fs::file_size(temp/"result.json")>1024*1024)fail(node.line,"Python result exceeds 1 MiB.");
    auto result=nlohmann::json::parse(readFile(temp/"result.json"));
    if(result.contains("error"))fail(node.line,"Python: "+result["error"].get<string>());
    Env updated;
    if(!check)for(auto& [name,original]:state){auto& value=result.at("values").at(name);
        if(holds_alternative<string>(original))updated[name]=value.get<string>();
        else if(holds_alternative<double>(original)){double number=value.get<double>();if(!isfinite(number))fail(node.line,"Python number is too large.");updated[name]=number;}
        else if(holds_alternative<bool>(original))updated[name]=value.get<bool>();
        else updated[name]=value.get<List>();
    }
    return updated;
}
}
