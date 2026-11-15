// Native release packaging. Uses the platform's compiler, archive and installer tools.
#include "../src/update_support.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
using namespace std;namespace fs=std::filesystem;
const string version=tappy_update::version;
void write(const fs::path& file,const string& text){fs::create_directories(file.parent_path());ofstream out(file,ios::binary);out<<text;if(!out)throw runtime_error("Cannot write "+file.string());}
void copyFile(const fs::path& from,const fs::path& to){fs::create_directories(to.parent_path());fs::copy_file(from,to,fs::copy_options::overwrite_existing);}
void copyTree(const fs::path& from,const fs::path& to){fs::create_directories(to);for(const auto& entry:fs::directory_iterator(from)){const auto name=entry.path().filename();if(name==".tappy-data"||name==".DS_Store"||name=="__pycache__")continue;if(entry.is_directory())copyTree(entry.path(),to/name);else if(entry.is_regular_file())copyFile(entry.path(),to/name);}}
fs::path command(const string& name){auto given=tappy_update::utf8(name);if(given.has_parent_path())return fs::absolute(given);
#ifdef _WIN32
    auto raw=_wgetenv(L"PATH");wstringstream paths(raw?raw:L"");wstring folder;while(getline(paths,folder,L';'))for(const auto& suffix:{L"",L".exe"}){auto path=fs::path(folder)/(given.wstring()+suffix);if(fs::is_regular_file(path))return path;}
#else
    auto raw=getenv("PATH");istringstream paths(raw?raw:"");string folder;while(getline(paths,folder,':')){auto path=fs::path(folder)/given;if(fs::is_regular_file(path))return path;}
#endif
    throw runtime_error("Missing tool: "+name);
}
void run(vector<string> args){vector<fs::path> paths{command(args.at(0))};for(size_t i=1;i<args.size();++i)paths.push_back(tappy_update::utf8(args[i]));if(tappy_update::process(paths)!=0)throw runtime_error("Command failed: "+args[0]);}
string text(const fs::path& path){auto native=path;native.make_preferred();auto value=native.u8string();return string(reinterpret_cast<const char*>(value.data()),value.size());}
void executable(const fs::path& path){fs::permissions(path,fs::perms::owner_all|fs::perms::group_read|fs::perms::group_exec|fs::perms::others_read|fs::perms::others_exec);}
string psquote(string value){string out="'";for(char c:value){out+=c;if(c=='\'')out+=c;}return out+"'";}
void archive(const fs::path& folder,const fs::path& output){fs::remove(output);
#ifdef _WIN32
    wchar_t system[32768];if(!GetSystemDirectoryW(system,32768))throw runtime_error("Cannot find PowerShell");
    run({text(fs::path(system)/"WindowsPowerShell/v1.0/powershell.exe"),"-NoProfile","-NonInteractive","-Command","$ErrorActionPreference='Stop'; Compress-Archive -Path "+psquote(text(folder/"*"))+" -DestinationPath "+psquote(text(output))+" -CompressionLevel Optimal"});
#else
    run({"/usr/bin/ditto","--norsrc","--noextattr","--noqtn","-c","-k",text(folder),text(output)});
#endif
}
void licenses(const fs::path& root,const fs::path& destination){copyFile(root/"LICENSE",destination/"Tappyweb-MIT.txt");copyFile(root/"vendor/httplib.LICENSE",destination/"cpp-httplib.txt");copyFile(root/"vendor/json.LICENSE",destination/"json.txt");copyFile(root/"assets/htmx.LICENSE",destination/"htmx.txt");}
void pe64(const fs::path& path){ifstream file(path,ios::binary);char magic[2]{};file.read(magic,2);file.seekg(60);uint32_t offset=0;file.read(reinterpret_cast<char*>(&offset),4);file.seekg(offset);char signature[4]{};uint16_t machine=0;file.read(signature,4);file.read(reinterpret_cast<char*>(&machine),2);if(!file||string(magic,2)!="MZ"||string(signature,4)!=string("PE\0\0",4)||machine!=0x8664)throw runtime_error("Expected Windows x64 PE: "+text(path));}
struct Temporary{fs::path path;Temporary(){path=fs::temp_directory_path()/("tappy-package-"+to_string(chrono::steady_clock::now().time_since_epoch().count()));if(!fs::create_directory(path))throw runtime_error("Cannot create package workspace");}~Temporary(){error_code error;fs::remove_all(path,error);}};
void windows(const fs::path& root,const map<string,string>& options){
    auto option=[&](string name,bool required=true){auto it=options.find(name);if(it==options.end()){if(required)throw runtime_error("Missing --"+name);return string{};}return it->second;};
    auto binary=tappy_update::utf8(option("binary")),updater=tappy_update::utf8(option("updater"));pe64(binary);pe64(updater);
    Temporary temp;auto payload=temp.path/"payload";fs::create_directory(payload);copyFile(binary,payload/"tappy.exe");copyFile(updater,payload/"tappy-update.exe");write(payload/"update-enabled",version+"\n");
    copyFile(root/"README.md",payload/"README.md");copyFile(root/"LICENSE",payload/"LICENSE");copyFile(root/"tools/install_windows.ps1",payload/"install.ps1");copyFile(root/"docs/windows.md",payload/"START-HERE.md");
    for(const auto* name:{"examples","docs"})copyTree(root/name,payload/name);licenses(root,payload/"licenses");
    if(auto license=option("toolchain-license",false);!license.empty())copyFile(tappy_update::utf8(license),payload/"licenses/llvm-mingw.txt");
    auto output=root/"releases"/("Tappyweb-"+version+"-windows-x64.zip");archive(payload,output);cout<<output<<'\n';
    auto helper=option("path-helper",false);if(helper.empty())return;pe64(tappy_update::utf8(helper));copyFile(tappy_update::utf8(helper),payload/"EnvPath.exe");copyFile(root/"assets/installer/AppIcon.ico",payload/"AppIcon.ico");fs::remove(payload/"install.ps1");fs::remove(payload/"START-HERE.md");
    vector<fs::path> files,folders;for(auto& entry:fs::recursive_directory_iterator(payload)){if(entry.is_regular_file())files.push_back(entry.path());else if(entry.is_directory())folders.push_back(entry.path());}if(files.empty())throw runtime_error("Empty installer payload");sort(files.begin(),files.end());sort(folders.begin(),folders.end(),[](const auto& a,const auto& b){return distance(a.begin(),a.end())>distance(b.begin(),b.end());});
    auto relative=[&](const fs::path& path){string value=text(fs::relative(path,payload)),escaped;for(char c:value){if(c=='$')escaped+='$';if(c=='"'||c=='\n'||c=='\r')throw runtime_error("Unsupported installer filename");escaped+=c=='/'?'\\':c;}return escaped;};
    string removal="; Remove only files owned by this installer.\n";for(auto& path:files)removal+="Delete \"$INSTDIR\\"+relative(path)+"\"\n";for(auto& path:folders)removal+="RMDir \"$INSTDIR\\"+relative(path)+"\"\n";write(temp.path/"remove-files.nsh",removal);
    auto nsis=option("makensis",false);if(nsis.empty())nsis="makensis";auto nsisDir=option("nsis-dir",false);if(!nsisDir.empty()){
#ifdef _WIN32
        _wputenv_s(L"NSISDIR",tappy_update::utf8(nsisDir).c_str());
#else
        setenv("NSISDIR",nsisDir.c_str(),1);
#endif
    }
#ifdef _WIN32
    string prefix="/D";
#else
    string prefix="-D";
#endif
    auto installer=root/"releases"/("Tappyweb-"+version+"-Windows-x64-Setup.exe");vector<string> args{nsis};for(auto& [key,value]:map<string,fs::path>{{"OUTPUT",installer},{"PAYLOAD",payload},{"PAYLOAD_FILES",payload/"*"},{"ART",root/"assets/installer/sidebar.bmp"},{"ICON",root/"assets/installer/AppIcon.ico"},{"REMOVE_FILES",temp.path/"remove-files.nsh"}})args.push_back(prefix+key+"="+text(value));args.push_back(text(root/"tools/nsis/installer.nsi"));run(args);cout<<installer<<'\n';
}
void macos(const fs::path& root){
#ifndef __APPLE__
    (void)root;throw runtime_error("macOS packages must be built on macOS.");
#else
    Temporary temp;auto payload=temp.path/"root",runtime=payload/"usr/local/libexec/tappyweb",binary=runtime/"tappy",helper=runtime/"tappy-update";fs::create_directories(runtime);
    const vector<string> flags={"clang++","-std=c++20","-O2","-pthread","-arch","arm64","-arch","x86_64","-mmacosx-version-min=13.0","-Ivendor"};
    auto args=flags;args.insert(args.end(),{"src/main.cpp","-o",text(binary)});run(args);args=flags;args.insert(args.end(),{"tools/updater.cpp","-o",text(helper)});run(args);
    for(auto& file:{binary,helper}){executable(file);run({"codesign","--force","--sign","-",text(file)});run({"codesign","--verify","--strict",text(file)});}write(runtime/"update-enabled",version+"\n");
    auto wrapper=payload/"usr/local/bin/tappy";write(wrapper,"#!/bin/sh\nexec /usr/local/libexec/tappyweb/tappy \"$@\"\n");executable(wrapper);write(payload/"etc/paths.d/tappyweb","/usr/local/bin\n");
    auto share=payload/"usr/local/share/tappyweb";for(auto* name:{"examples","docs"})copyTree(root/name,share/name);copyFile(root/"README.md",share/"README.md");copyFile(root/"LICENSE",share/"LICENSE");licenses(root,share/"licenses");
    auto app=payload/"Applications/Tappyweb.app",mac=app/"Contents/MacOS",resources=app/"Contents/Resources";fs::create_directories(mac);fs::create_directories(resources);
    for(auto* arch:{"arm64","x86_64"})run({"swiftc","-O","-target",string(arch)+"-apple-macosx13.0","-framework","AppKit","-framework","CoreServices","tools/macos_launcher.swift","-o",text(temp.path/(string("launcher-")+arch))});
    run({"lipo","-create",text(temp.path/"launcher-arm64"),text(temp.path/"launcher-x86_64"),"-output",text(mac/"TappywebLauncher")});
    auto iconset=temp.path/"Tappy.iconset";fs::create_directory(iconset);for(int size:{16,32,128,256,512})for(int scale:{1,2})run({"sips","-z",to_string(size*scale),to_string(size*scale),text(root/"assets/installer/Logo.png"),"--out",text(iconset/("icon_"+to_string(size)+"x"+to_string(size)+(scale==2?"@2x":"")+".png"))});
    run({"iconutil","-c","icns",text(iconset),"-o",text(resources/"AppIcon.icns")});
    write(app/"Contents/Info.plist",R"XML(<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>CFBundleExecutable</key><string>TappywebLauncher</string><key>CFBundleIdentifier</key><string>dev.tappyweb.launcher</string><key>CFBundleName</key><string>Tappyweb</string><key>CFBundlePackageType</key><string>APPL</string><key>CFBundleVersion</key><string>)XML"+version+R"XML(</string><key>CFBundleShortVersionString</key><string>)XML"+version+R"XML(</string><key>CFBundleIconFile</key><string>AppIcon.icns</string><key>LSMinimumSystemVersion</key><string>13.0</string><key>LSUIElement</key><true/><key>CFBundleDocumentTypes</key><array><dict><key>CFBundleTypeName</key><string>Tappyweb source</string><key>CFBundleTypeRole</key><string>Viewer</string><key>LSHandlerRank</key><string>Owner</string><key>LSItemContentTypes</key><array><string>dev.tappyweb.source</string></array></dict></array><key>UTExportedTypeDeclarations</key><array><dict><key>UTTypeIdentifier</key><string>dev.tappyweb.source</string><key>UTTypeDescription</key><string>Tappyweb source</string><key>UTTypeConformsTo</key><array><string>public.plain-text</string></array><key>UTTypeTagSpecification</key><dict><key>public.filename-extension</key><array><string>tappy</string></array></dict></dict></array></dict></plist>)XML");run({"codesign","--force","--sign","-",text(app)});
    auto scripts=temp.path/"scripts";write(scripts/"postinstall",R"SH(#!/bin/sh
set -eu
TARGET_VOLUME="${3:-/}"
/bin/chmod 755 "$TARGET_VOLUME/usr/local/bin/tappy" "$TARGET_VOLUME/usr/local/libexec/tappyweb/tappy" "$TARGET_VOLUME/usr/local/libexec/tappyweb/tappy-update"
"$TARGET_VOLUME/usr/local/libexec/tappyweb/tappy" --version >/dev/null
if [ "$TARGET_VOLUME" = / ]; then
    /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f /Applications/Tappyweb.app
    CONSOLE_USER=$(/usr/bin/stat -f%Su /dev/console)
    if [ "$CONSOLE_USER" != root ] && [ "$CONSOLE_USER" != loginwindow ]; then
        CONSOLE_UID=$(/usr/bin/id -u "$CONSOLE_USER")
        /bin/launchctl asuser "$CONSOLE_UID" /usr/bin/sudo -u "$CONSOLE_USER" /Applications/Tappyweb.app/Contents/MacOS/TappywebLauncher --register || true
    fi
fi
exit 0
)SH");executable(scripts/"postinstall");
    auto portable=temp.path/"portable";copyTree(runtime,portable);licenses(root,portable/"licenses");copyFile(root/"LICENSE",portable/"LICENSE");archive(portable,root/"releases"/("Tappyweb-"+version+"-macOS-universal.zip"));
    auto output=root/"releases"/("Tappyweb-"+version+"-macOS-universal.pkg");run({"pkgbuild","--root",text(payload),"--identifier","dev.tappyweb.cli","--version",version,"--install-location","/","--ownership","recommended","--scripts",text(scripts),text(output)});cout<<output<<'\n';
#endif
}
void linuxPackage(const fs::path& root,const map<string,string>& options){
#ifndef __linux__
    (void)root;(void)options;throw runtime_error("Linux packages must be built on Linux.");
#else
#if defined(__aarch64__)
    const string arch="arm64",debArch="arm64";
#elif defined(__x86_64__)
    const string arch="x64",debArch="amd64";
#else
#error Unsupported Linux architecture
#endif
    auto option=[&](const string& name){auto it=options.find(name);if(it==options.end())throw runtime_error("Missing --"+name);return tappy_update::utf8(it->second);};
    Temporary temp;auto portable=temp.path/"portable";fs::create_directory(portable);
    for(auto name:{"binary","updater"}){auto file=portable/(string(name)=="binary"?"tappy":"tappy-update");copyFile(option(name),file);executable(file);}
    write(portable/"update-enabled",version+"\n");copyFile(root/"README.md",portable/"README.md");copyFile(root/"LICENSE",portable/"LICENSE");licenses(root,portable/"licenses");
    for(auto name:{"docs","examples"})copyTree(root/name,portable/name);
    write(portable/"install.sh",R"SH(#!/bin/sh
set -eu
SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PREFIX="${TAPPY_PREFIX:-$HOME/.local}"
mkdir -p "$PREFIX/lib/tappyweb" "$PREFIX/bin"
cp -R "$SOURCE/." "$PREFIX/lib/tappyweb/"
ln -sf "$PREFIX/lib/tappyweb/tappy" "$PREFIX/bin/tappy"
printf 'Installed Tappyweb. Add %s/bin to PATH if needed.\n' "$PREFIX"
)SH");executable(portable/"install.sh");
    auto output=root/"releases"/("Tappyweb-"+version+"-linux-"+arch+".tar.gz");run({"tar","-czf",text(output),"-C",text(portable),"."});cout<<output<<'\n';
    auto payload=temp.path/"deb",runtime=payload/"usr/lib/tappyweb";copyTree(portable,runtime);fs::remove(runtime/"install.sh");fs::create_directories(payload/"usr/bin");fs::create_symlink("../lib/tappyweb/tappy",payload/"usr/bin/tappy");
    write(payload/"usr/share/applications/tappyweb.desktop",R"DESKTOP([Desktop Entry]
Type=Application
Name=Tappyweb
Comment=Run a Tappy web app
Exec=tappy %f
Terminal=true
Icon=tappyweb
MimeType=text/x-tappy;
Categories=Development;WebDevelopment;
NoDisplay=true
)DESKTOP");
    write(payload/"usr/share/mime/packages/tappyweb.xml",R"XML(<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info"><mime-type type="text/x-tappy"><comment>Tappyweb source</comment><sub-class-of type="text/plain"/><glob pattern="*.tappy"/></mime-type></mime-info>
)XML");copyFile(root/"assets/logo.png",payload/"usr/share/icons/hicolor/256x256/apps/tappyweb.png");
    // Debian derives the actual glibc/libstdc++ requirements from the built binary.
    write(payload/"debian/control","Source: tappyweb\nSection: devel\nPriority: optional\nMaintainer: TappyWeb contributors <noreply@tappyweb.dev>\n\nPackage: tappyweb\nArchitecture: "+debArch+"\nDescription: Readable web language with a C++ runtime and HTMX\n");
    auto before=fs::current_path();fs::current_path(payload);run({"dpkg-shlibdeps","-e"+text(runtime/"tappy"),"-e"+text(runtime/"tappy-update"),"-T"+text(temp.path/"substvars")});fs::current_path(before);
    // dpkg-shlibdeps needs debian/control in its working directory.
    ifstream dependencies(temp.path/"substvars");string line,depends;while(getline(dependencies,line))if(line.rfind("shlibs:Depends=",0)==0)depends=line.substr(15);
    if(depends.empty())throw runtime_error("Cannot determine Linux runtime dependencies");
    write(payload/"DEBIAN/control","Package: tappyweb\nVersion: "+version+"\nSection: devel\nPriority: optional\nArchitecture: "+debArch+"\nMaintainer: TappyWeb contributors <noreply@tappyweb.dev>\nDepends: "+depends+", curl, ca-certificates, tar, coreutils\nRecommends: shared-mime-info, desktop-file-utils\nDescription: Readable web language with a C++ runtime and HTMX\n");
    for(auto name:{"postinst","postrm"}){write(payload/"DEBIAN"/name,"#!/bin/sh\nset -e\nif command -v update-mime-database >/dev/null; then update-mime-database /usr/share/mime; fi\nif command -v update-desktop-database >/dev/null; then update-desktop-database /usr/share/applications; fi\n");executable(payload/"DEBIAN"/name);}
    fs::remove_all(payload/"debian");auto deb=root/"releases"/("Tappyweb-"+version+"-linux-"+arch+".deb");run({"dpkg-deb","--root-owner-group","--build",text(payload),text(deb)});cout<<deb<<'\n';
#endif
}
int packageMain(int argc,char** argv){try{if(argc<2){cout<<"tappy-package macos|windows|linux [--root path] [--binary file --updater file --path-helper file --makensis file --nsis-dir path --toolchain-license file]\n";return 0;}string mode=argv[1];map<string,string> options;for(int i=2;i<argc;i+=2){if(i+1>=argc||string(argv[i]).rfind("--",0)!=0)throw runtime_error("Expected --option value");string key=string(argv[i]).substr(2),value=argv[i+1];if(key!="makensis"||tappy_update::utf8(value).has_parent_path())value=text(fs::absolute(tappy_update::utf8(value)));options[key]=value;}auto root=options.count("root")?tappy_update::utf8(options.at("root")):fs::current_path();fs::current_path(root);fs::create_directories(root/"releases");if(mode=="macos")macos(root);else if(mode=="windows")windows(root,options);else if(mode=="linux")linuxPackage(root,options);else throw runtime_error("Use macos, windows or linux.");return 0;}catch(const exception& error){cerr<<error.what()<<'\n';return 1;}}
#ifdef _WIN32
int wmain(int argc,wchar_t** wide){vector<string> values;vector<char*> args;for(int i=0;i<argc;++i){int size=WideCharToMultiByte(CP_UTF8,0,wide[i],-1,nullptr,0,nullptr,nullptr);string value(size_t(size),0);WideCharToMultiByte(CP_UTF8,0,wide[i],-1,value.data(),size,nullptr,nullptr);value.pop_back();values.push_back(std::move(value));}for(auto& value:values)args.push_back(value.data());return packageMain(argc,args.data());}
#else
int main(int argc,char** argv){return packageMain(argc,argv);}
#endif
