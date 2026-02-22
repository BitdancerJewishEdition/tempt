#include <iostream>
#include <string>
#include <memory>
#include <array>
#include <cstdio>
#include "process.hpp"
#include "Offsets.h"

int findRobloxPid() 
{
    std::array<char, 256> buffer;
    std::string result;
    std::string cmd = "ps -eo pid,comm,%cpu | grep ' Main ' | sort -k3 -rn | head -1 | awk '{print $1}'";

    auto deleter = [](FILE* f) { if (f) pclose(f); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(popen(cmd.c_str(), "r"), deleter);
    
    if (!pipe) 
    {
        return -1;
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) 
    {
        result += buffer.data();
    }
    
    if (!result.empty()) 
    {
        try {
            return std::stoi(result);
        } catch (...) 
        {
            return -1;
        }
    }
    
    return -1;
}

auto main() -> int
{
    int pid = findRobloxPid();
    Process proc(pid);
    std::cout << "-> Roblox PID: " << pid << std::endl;

    uintptr_t VisualEngine = proc.read<uintptr_t>(proc.base + Offsets::VisualEnginePointer);
    const auto FakeDatamodel = proc.find_class_offset(VisualEngine, "DataModel");

    if (FakeDatamodel)
    {
        std::cout << "-> VisualEngine::ToFakeDataModel = 0x" << std::hex << *FakeDatamodel << std::endl;
    }

    const auto FakeDatamodelPointer = proc.read<uintptr_t>(VisualEngine + *FakeDatamodel);
    const auto Datamodel = proc.find_class_offset(FakeDatamodelPointer, "DataModel");

    if (Datamodel){
        std::cout << "-> FakeDataModel::ToRealDataModel = 0x" << std::hex << *Datamodel << std::endl;
    }

    return 1; 
}
