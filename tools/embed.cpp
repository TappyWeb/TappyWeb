#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
int main(int argc,char** argv) {
    try {
        namespace fs=std::filesystem;
        const fs::path root=argc>1?fs::path(argv[1]):fs::current_path();
        std::string output="#pragma once\n#include <string_view>\n";
        for(const auto& [name,path]:{std::pair{"htmx_js","assets/htmx.min.js"},std::pair{"default_css","assets/default.css"}}) {
            std::ifstream file(root/path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read asset: "+std::string(path));
            std::string content{std::istreambuf_iterator<char>(file),{}};
            std::string delimiter="TAPPY_ASSET";while(content.find(")"+delimiter+"\"")!=std::string::npos)delimiter+="_";
            output+="inline constexpr std::string_view "+std::string(name)+" = R\""+delimiter+"("+content+")"+delimiter+"\";\n";
        }
        std::ofstream file(root/"src/assets.hpp",std::ios::binary);file<<output;if(!file)throw std::runtime_error("Cannot write assets.hpp");
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
