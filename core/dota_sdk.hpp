#include <BlackBone/Asm/AsmFactory.h>
#include <BlackBone/Asm/AsmHelper64.h>
#include <BlackBone/Asm/IAsmHelper.h>
#include <BlackBone/Patterns/PatternSearch.h>
#include <BlackBone/Process/Process.h>
#include <BlackBone/Process/ProcessMemory.h>
#include <BlackBone/Process/RPC/RemoteFunction.hpp>
#include <BlackBone/Process/RPC/RemoteHook.h>
#include <format>
#include <iostream>

#pragma warning(disable : 6301)

class DefClass {
  public:
    blackbone::ProcessMemory *memory;
    std::uintptr_t baseAddr;

    std::uintptr_t GetVF(unsigned short idx) noexcept {
        const auto VMTAddr = memory->Read<std::uintptr_t>(baseAddr).result();

        return memory->Read<std::uintptr_t>(VMTAddr + idx * 8).result();
    }

    bool IsValid() { return (memory && baseAddr) ? true : false; }
};

std::uintptr_t GetAbsoluteAddress(blackbone::ProcessMemory *mem,
                                  std::uintptr_t instruction_ptr,
                                  int offset = 3, int size = 7) {
    return instruction_ptr +
           (mem->Read<std::int32_t>(instruction_ptr + offset).result()) + size;
}

class CDOTA_Camera : public DefClass {
  public:
    CDOTA_Camera() {
        memory = nullptr;
        baseAddr = NULL;
    }

    CDOTA_Camera(blackbone::ProcessMemory *memory, std::uintptr_t baseAddr) {
        this->memory = memory;
        this->baseAddr = baseAddr;
    }

    void SetDistance(float distance)  {
        memory->Write<float>(baseAddr + 0x2e4, distance);
    }

    void SetFOWAmount(float amount) {
        memory->Write<float>(baseAddr + 0x70, amount);
    }

    auto GetDistance() {
        return memory->Read<float>(baseAddr + 0x270).result();
    }

    auto GetFOWAmount() {
        return memory->Read<float>(baseAddr + 0x70).result();
    }

    void ToggleFog() {
        const auto aGetFog = this->GetVF(18);
        const auto instructionBytes = memory->Read<uintptr_t>(aGetFog).result();

        if (instructionBytes == 0x83485708245c8948) { // not patched

            // 0x0F, 0x57, 0xC0 | xorps xmm0, xmm0
            // 0xC3				| ret
            constexpr const char *bytePatch = "\x0F\x57\xC0\xC3";
            memory->Write(aGetFog, 4, bytePatch);
            // std::cout << "Fog instructions patched" << std::endl;
        } else if (instructionBytes == 0x83485708c3c0570f) { // already patched

            // 0x48, 0x89, 0x5C, 0x24, 0x08 | mov qword ptr ss:[rsp+8], rbx
            // 0x57							| push rdi
            constexpr const char *byteRestore = "\x48\x89\x5C\x24\x08\x57";
            memory->Write(aGetFog, 6, byteRestore);
            // std::cout << "Fog instructions restored" << std::endl;
        } else {
            std::cout << "Error, unknown fog instructions: " << instructionBytes
                      << std::endl;
            std::system("pause");
            exit(1);
        }
    }

    void ToggleMaxZFar() {
        const auto aGetZFar = this->GetVF(19);
        const auto instructionBytes =
            memory->Read<uintptr_t>(aGetZFar).result();

        if (instructionBytes == 0x83485708245c8948) { // not patched

            // 0xB8, 0x50, 0x46, 0x00, 0x00	| mov eax, 18000
            // 0xF3, 0x0F, 0x2A, 0xC0		| cvtsi2ss xmm0, eax
            // 0xC3							| ret
            constexpr const char *bytePatch =
                "\xB8\x50\x46\x00\x00\xF3\x0F\x2A\xC0\xC3";
            memory->Write(aGetZFar, 10, bytePatch);
            // std::cout << "ZFar instructions patched" << std::endl;
        } else if (instructionBytes == 0x2a0ff300004650b8) { // already patched

            // 0x48, 0x89, 0x5C, 0x24, 0x08 | mov qword ptr ss:[rsp+8], rbx
            // 0x57							| push rdi
            // 0x48, 0x83, 0xEC, 0x40       | sub rsp, 40
            constexpr const char *byteRestore =
                "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x40";
            memory->Write(aGetZFar, 10, byteRestore);
            // std::cout << "ZFar instructions restored" << std::endl;
        }
    }
};

CDOTA_Camera FindCamera(blackbone::Process &proc) {
    // std::vector<blackbone::ptr_t> search_result;
    // typedef std::uintptr_t( __fastcall* CDOTACamera__Init )( );
    // blackbone::PatternSearch aDOTACameraInit_Pattern{
    // "\x48\x83\xEC\x38\xE8\xCC\xCC\xCC\xCC\x48\x85\xC0\x74\x4D" };
    // aDOTACameraInit_Pattern.SearchRemote( proc, 0xCC, proc.modules(
    // ).GetModule( L"client.dll" ).get( )->baseAddress, proc.modules(
    // ).GetModule( L"client.dll" ).get( )->size, search_result, 1 );
    // blackbone::RemoteFunction<CDOTACamera__Init> pFN( proc,
    // search_result.front( ) );

    // if ( auto result = pFN.Call( ); result.success( ) && result.result( ) ) {
    //	return CDOTA_Camera{ &proc.memory( ), result.result( ) };
    // }

    // return CDOTA_Camera{ nullptr, 0 };

    static std::vector<blackbone::ptr_t> search_result;
    static bool first_use = true;
    if (first_use) {
        blackbone::PatternSearch g_pDOTACameraManager_pattern{
            "\x48\x8d\x3d\xcc\xcc\xcc\xcc\x48\x8b\x14\xc8"};
        g_pDOTACameraManager_pattern.SearchRemote(
            proc, 0xCC,
            proc.modules().GetModule(L"client.dll").get()->baseAddress,
            proc.modules().GetModule(L"client.dll").get()->size, search_result,
            1);
        first_use = false;
    }

    if (search_result.empty()) {
        std::cout << "cant find dota camera\n";
        getchar();
        exit(1);
    }

    if (std::uintptr_t g_pDOTACameraManager =
            GetAbsoluteAddress(&proc.memory(), search_result.front());
        g_pDOTACameraManager) {
        auto dotaCamera =
            proc.memory().Read<std::uintptr_t>(g_pDOTACameraManager + 0x20);

        if (dotaCamera.success() && dotaCamera.result())
            return CDOTA_Camera{&proc.memory(), dotaCamera.result()};
    }

    return CDOTA_Camera{nullptr, 0};
}