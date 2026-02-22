// lib by hadi (@h6di)
#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <sys/uio.h>
#include <cstring>
#include <vector>
#include <cstdint>
#include <optional>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>

struct ModuleRange {
    uintptr_t base;
    uintptr_t end;
};

struct RttiInfo {
    std::string name;
    uintptr_t typeinfo_addr;
};

static ModuleRange find_roblox_module(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream maps(path);
    std::string line;

    uintptr_t best_base = 0;
    uintptr_t best_end = 0;
    size_t largest_exec_size = 0;

    while (std::getline(maps, line)) {
        uintptr_t start, end;
        char perms[5];
        if (sscanf(line.c_str(), "%lx-%lx %4s", &start, &end, perms) != 3)
            continue;

        size_t region_size = end - start;

        if (perms[2] == 'x' && region_size > 10 * 1024 * 1024) { 
            uint32_t magic = 0;
            struct iovec local = {&magic, 4};
            struct iovec remote = {(void*)start, 4};
            
            if (process_vm_readv(pid, &local, 1, &remote, 1, 0) == 4) {
                if (magic == 0x464c457f) { // \x7fELF
                    if (region_size > largest_exec_size) {
                        largest_exec_size = region_size;
                        best_base = start;
                        best_end = end; 
                    }
                }
            }
        }
        
        if (best_base != 0 && start >= best_end && start <= best_end + 0x1000) {
            best_end = end;
        }
    }
    
    return {best_base, best_end};
}

class Process
{
public:
    ModuleRange mod_range;
    uintptr_t base;
private:
    int m_pid;

    inline bool check_range(uintptr_t address) const
    {
        if ((address >= mod_range.base && address <= mod_range.end) || (address >= 0x100000000000 && address <= 0x7fffffffffff))
        {
            return false;
        }
        
        //std::cerr << "address out of range: 0x" << std::hex << address << std::endl;
        return true; 
    }
public:
    Process(int pid) : m_pid(pid)
    {
        mod_range = find_roblox_module(pid);
        base = mod_range.base;
    }

    int get_pid() const { return m_pid; }

    template<typename T>
    T read(uintptr_t address) const
    {
        if (check_range(address)) return 0;
        T value;
        struct iovec local = {&value, sizeof(T)};
        struct iovec remote = {(void*)address, sizeof(T)};
        
        ssize_t nread = process_vm_readv(m_pid, &local, 1, &remote, 1, 0);
        
        if (nread != sizeof(T)) {
            std::cerr << "failed to read mem at addr 0x" << std::hex << address << std::endl;
            return 0x0;
        }
        
        return value;
    }

    template<typename T>
    void write(uintptr_t address, const T& value) const
    {
        if (check_range(address)) return;
        struct iovec local = {(void*)&value, sizeof(T)};
        struct iovec remote = {(void*)address, sizeof(T)};
        
        ssize_t nwritten = process_vm_writev(m_pid, &local, 1, &remote, 1, 0);
        
        if (nwritten != sizeof(T)) {
            std::cerr << "failed to write mem at addr 0x" << std::hex << address << std::endl;
        }
    }

    std::vector<uint8_t> read_bytes(uintptr_t address, size_t size) const
    {
        if (check_range(address)) return std::vector<uint8_t>();
        std::vector<uint8_t> buffer(size);
        struct iovec local = {buffer.data(), size};
        struct iovec remote = {(void*)address, size};
        
        ssize_t nread = process_vm_readv(m_pid, &local, 1, &remote, 1, 0);
        
        if (nread <= 0) {
            std::cerr << "failed to read bytes at mem addr 0x" << std::hex << address << std::endl;
        }
        
        buffer.resize(nread);
        return buffer;
    }

    void write_bytes(uintptr_t address, const std::vector<uint8_t>& data) const
    {
        if (check_range(address)) return;
        struct iovec local = {(void*)data.data(), data.size()};
        struct iovec remote = {(void*)address, data.size()};
        
        ssize_t nwritten = process_vm_writev(m_pid, &local, 1, &remote, 1, 0);
        
        if (nwritten != (ssize_t)data.size()) {
            std::cerr << "failed to write bytes at mem addr 0x" << std::hex << address << std::endl;
        }
    }

    std::string read_string(uintptr_t address, size_t max_length = 256) const
    {
        if (check_range(address)) return std::string("");
        std::vector<char> buffer(max_length);
        struct iovec local = {buffer.data(), max_length};
        struct iovec remote = {(void*)address, max_length};
        
        ssize_t nread = process_vm_readv(m_pid, &local, 1, &remote, 1, 0);
        
        if (nread <= 0) {
            // std::cerr << "failed to read string at mem addr 0x" << std::hex << address << std::endl;
            return std::string("");
        }
        
        for (ssize_t i = 0; i < nread; ++i) {
            if (buffer[i] == '\0') {
                return std::string(buffer.data(), i);
            }
        }
        
        return std::string(buffer.data(), nread);
    }

    std::string rbx_read_string(uintptr_t address, size_t max_length = 256) const
    {
        if (check_range(address)) return std::string("");
        std::vector<char> buffer(max_length);
        struct iovec local = {buffer.data(), max_length};
        struct iovec remote = {(void*)address, max_length};
        
        ssize_t nread = process_vm_readv(m_pid, &local, 1, &remote, 1, 0);
        
        if (nread <= 0) {
            // std::cerr << "failed to read string at mem addr 0x" << std::hex << address << std::endl;
            return std::string("");
        }
        
        for (ssize_t i = 0; i < nread; ++i) {
            if (buffer[i] == '\0') {
                return std::string(buffer.data(), i);
            }
        }
        
        return std::string(buffer.data(), nread);
    }

    uintptr_t find_pattern(const std::string& pattern) const
    {
        std::vector<int> pattern_bytes;
        std::stringstream ss(pattern);
        std::string hex;
        while (ss >> hex) {
            if (hex == "?" || hex == "??") {
                pattern_bytes.push_back(-1);
            } else {
                pattern_bytes.push_back(std::stoi(hex, nullptr, 16));
            }
        }

        size_t size = mod_range.end - mod_range.base;
        std::vector<uint8_t> buffer = read_bytes(mod_range.base, size);
        if (buffer.empty())
        {
            std::cerr << "buffer empty" << std::endl;
            return 0;
        }

        auto it = std::search(buffer.begin(), buffer.end(),
            pattern_bytes.begin(), pattern_bytes.end(),
            [](uint8_t memory_byte, int pattern_byte) {
                return pattern_byte == -1 || memory_byte == pattern_byte;
            });

        if (it != buffer.end()) {
            return mod_range.base + std::distance(buffer.begin(), it);
        }
        return 0;
    }

    std::vector<uintptr_t> find_xrefs(uintptr_t target_address, uintptr_t search_start, uintptr_t search_end) const
    {
        std::vector<uintptr_t> xrefs;
        size_t scan_size = search_end - search_start;
        
        std::vector<uint8_t> buffer = read_bytes(search_start, scan_size);
        
        for (size_t i = 0; i <= buffer.size() - sizeof(uintptr_t); i += 8) {
            uintptr_t potential_ptr;
            std::memcpy(&potential_ptr, &buffer[i], sizeof(uintptr_t));
            
            if (potential_ptr == target_address) {
                xrefs.push_back(search_start + i);
            }
        }
        return xrefs;
    }

    auto scan_rtti(uintptr_t instance_ptr) const -> std::optional<RttiInfo>
    {
        uintptr_t vtable = read<uintptr_t>(instance_ptr);
        if (vtable < 0x1000) return std::nullopt;

        uintptr_t typeinfo_ptr = read<uintptr_t>(vtable - 0x8);
        if (typeinfo_ptr < 0x1000) return std::nullopt;

        uintptr_t name_ptr = read<uintptr_t>(typeinfo_ptr + 0x8);
        if (name_ptr < 0x1000) return std::nullopt;

        std::string mangled_name = read_string(name_ptr);
        if (mangled_name.empty()) return std::nullopt;

        RttiInfo info;
        info.name = mangled_name;
        info.typeinfo_addr = typeinfo_ptr;
        return info;
    }

    auto find_class_offset(uintptr_t base_instance, const std::string& target_substring, size_t max_offset = 0x1500) const -> std::optional<size_t> 
    {
        for (size_t offset = 0; offset < max_offset; offset += 8) {
            uintptr_t member_ptr = read<uintptr_t>(base_instance + offset);
            
            if (member_ptr < 0x1000) continue;

            auto rtti = scan_rtti(member_ptr);
            if (rtti && rtti->name.find(target_substring) != std::string::npos) {
                return offset;
            }
        }
        return std::nullopt;
    }
};

#endif
