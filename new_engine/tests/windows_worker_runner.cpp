// Host-side Wine harness for the same redirected process path used by Studio.
#include "engine/world_platform.h"
#include <iostream>
int main(int argc,char **argv){
    std::vector<std::string> arguments;
    for(int i=1;i<argc;i++)arguments.emplace_back(argv[i]);
    std::string output,error;int code=vw_run_process(arguments,output,error);
    std::cout<<output;if(!error.empty())std::cerr<<error<<'\n';return code<0?1:code;
}
