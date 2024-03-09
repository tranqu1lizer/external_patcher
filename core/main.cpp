#include "dota_sdk.hpp"

[[nodiscard]] inline int req_action(byte w) {
    if (w == 0) {
        int act1;

        std::cout << "[!] - patches .text section\n============================\n[1] Change camera "
                     "distance\n[2] "
                     "Patch ZFar [!]\n[3] Toggle Fog [!]\n[4] Toggle mana bars [!]\n[5] Toggle particles [!]\n=> ";
        std::cin >> act1;
        return act1;
    } else {
        int dist;
        std::system("cls");
        std::cout << "=> ";
        std::cin >> dist;

        return dist;
    }
}

void process(blackbone::Process &dota_proc,
             blackbone::ProcessMemory &pDOTAMemory) {
    static const auto client_module =
        dota_proc.modules().GetModule(L"client.dll").get()->baseAddress;
    static const auto client_size =
        dota_proc.modules().GetModule(L"client.dll").get()->size;

    std::cout << "PID: " << std::dec << dota_proc.pid() << std::endl
              << "client.dll base: " << (void *)client_module << std::endl;

    auto DOTACamera = FindCamera(dota_proc);

    // EB, 0E | jmp client.7FF9A7BBCFFA
    constexpr const char *bytePatch = "\xEB\x0E";
    // 75, 0E | jne client.7FF9A7BBCFFA
    constexpr const char *byteRestore = "\x75\x0E";

    static uint64_t manabar_addr = 0;
    static uint64_t particles_addr = 0;
    uint64_t insn_bytes = 0, insn_bytes2 = 0;

    static bool first = true;
    if (first) {
        // 75 cc c6 44 24 cc cc eb cc 48 8d 9e
        blackbone::PatternSearch jmp_insn_manabar{
            "\x75\xcc\xc6\x44\x24\xcc\xcc\xeb\xcc\x48\x8d\x9e"},
            jmp_insn_manabar2{
                "\xeb\xcc\xc6\x44\x24\xcc\xcc\xeb\xcc\x48\x8d\x9e"};
        std::vector<blackbone::ptr_t> search_result, search_result2;

        jmp_insn_manabar.SearchRemote(dota_proc, 0xCC, client_module,
                                      client_size, search_result, 1);
        jmp_insn_manabar2.SearchRemote(dota_proc, 0xCC, client_module,
                                       client_size, search_result2, 1);

        if (!search_result.empty())
            manabar_addr = search_result.front();
        else if (!search_result2.empty())
            manabar_addr = search_result2.front();

        search_result.clear();

        blackbone::PatternSearch and_al_01{"\x4C\x8B\xDC\x55\x56\x41\x54"};
        and_al_01.SearchRemote(
            dota_proc, 0xCC,
            dota_proc.modules().GetModule(L"particles.dll").get()->baseAddress,
            dota_proc.modules().GetModule(L"particles.dll").get()->size,
            search_result, 1);

        if (search_result.empty()) {
            std::cout << "cant find particles pattern\n";
            getchar();
            exit(1);
        }

        particles_addr = search_result.front() + 0x1a;

        first = false;
    }

    if (DOTACamera.IsValid()) {
        const auto act = req_action(0);

        switch (act) {
        case 1:
            DOTACamera.SetDistance(req_action(1));
            break;
        case 2:
            DOTACamera.ToggleMaxZFar();
            break;
        case 3:
            DOTACamera.ToggleFog();
            break;
        case 4:
            if (!manabar_addr) {
                std::cout << "cant find manabar pattern\n";
                getchar();
                exit(1);
            }

            insn_bytes = pDOTAMemory.Read<uintptr_t>(manabar_addr).result();
            if (insn_bytes == 0xeb00542444c60e75) {
                pDOTAMemory.Write(manabar_addr, 2, bytePatch);
            } else if (insn_bytes == 0xeb00542444c60eeb) { // restore
                pDOTAMemory.Write(manabar_addr, 2, byteRestore);
            } else {
                std::cout << "[[unknown bytes at " << std::hex << manabar_addr
                          << '\n';
                getchar();
                exit(1);
            }

            break;
        case 5:
            insn_bytes2 = pDOTAMemory.Read<USHORT>(particles_addr)
                              .result();
            if (insn_bytes2 == 0x0124) {
                pDOTAMemory.Write(particles_addr, 2, "\xb2\x01");
            } else if (insn_bytes2 == 0x01b2) {
                pDOTAMemory.Write(particles_addr, 2, "\x24\x01");
            } else {
                std::cout << "[unknown bytes at " << std::hex << manabar_addr
                          << '\n';
                getchar();
                exit(1);
            }
            break;
        default:
            exit(1);
        }
    }
    std::system("cls");
}

int main() {
    blackbone::Process dota;

    if (NT_SUCCESS(dota.Attach(L"dota2.exe")) &&
        dota.modules().GetModule(L"client.dll")) {
        std::cout << "Attached to dota2.exe " << std::endl;
        while (1)
            process(dota, dota.memory());
    } else {
        std::cout << "dota2.exe not found or not client.dll" << std::endl;
        std::system("pause");
        exit(1);
    }
}
