#include "color.hpp"
#include "dota_sdk.hpp"

__int64 __fastcall GetBaseEntity(blackbone::ProcessMemory& a0, uintptr_t a1, int a2)
{
	__int64 v2; // rcx
	uintptr_t v3; // rcx

	if ((unsigned int)a2 <= 0x7FFE
		&& (unsigned int)(a2 >> 9) <= 0x3F)
	{
		auto hui2 = (v2 = *a0.Read<__int64>(a1 + 8i64 * (a2 >> 9) + 16));
		if (hui2 != 0) {
			auto hui = (v3 = (120i64 * (a2 & 0x1FF) + v2));
			if (hui != 0i64) {
				return *a0.Read<__int64>(v3);
			}
		}
	}
	else
	{
		return 0i64;
	}
}

const char* GetEntityName(blackbone::ProcessMemory& mem, uint64_t ident) {
	char* buf = (char*)malloc(0x40);
	memset(buf, 0x0, 0x40);

	auto m_name = mem.Read<uint64_t>(ident + 0x18);
	if (m_name.success() && *m_name)
		mem.Read(*m_name, 0x40, buf);

	auto m_designerName = mem.Read<uint64_t>(ident + 0x20);
	if (m_designerName.success() && *m_designerName)
		mem.Read(*m_designerName, 0x40, buf);

	return buf;
};


[[nodiscard]] inline static int req_action(byte w) {
	if (w == 0) {
		int act1;
		std::cout << dye::aqua_on_blue("Dota 2 External Patcher") << "\n["
			<< dye::light_aqua("https://github.com/tranqu1lizer/") << "]\n["
			<< dye::light_aqua("https://yougame.biz/threads/288873")
			<< "]\n\n";
		std::cout << dye::black_on_yellow("Warning: [!] - use at own risk")
			<< "\n\n[1] Change camera distance\n"
			<< "[2] Patch ZFar\n"
			<< "[3] Toggle Fog\n"
			<< "[4] " << dye::black_on_yellow("[!]") << " Toggle particles\n"
			<< "[5] " << dye::black_on_yellow("[!]") << " Unlock/lock emoticon\n"
			<< "[6] " << dye::black_on_yellow("[!]") << " Change weather(partially) "
			//<< "\n[7] " << dye::black_on_yellow("[!]") << " VBE "
			<< "\n=> ";
		std::cin >> act1;
		return act1;
	}
	else if (w == 1) {
		int dist;
		std::system("cls");
		std::cout << "=> ";
		std::cin >> dist;

		return dist;
	}
	else if (w == 2) {
		int value;

		std::system("cls");
		std::cout << "Weather number(0-9, 0 = restore default)\n=> ";
		std::cin >> value;

		return value;
	}
}

void toggle_fog_farz(blackbone::ProcessMemory& mem, uint64_t ppGameEntitySystem,bool isfarz = false) {


	uint64_t pGameEntitySystem = *mem.Read<uint64_t>(ppGameEntitySystem);
	uint64_t identity = *mem.Read<uint64_t>(pGameEntitySystem + 0x210);

	int highestIndex = *mem.Read<int>(pGameEntitySystem + 0x2100);

	for (int i = 0; i <= highestIndex; ++i) {
		uint64_t ent = GetBaseEntity(mem, pGameEntitySystem, i);
		if (!ent)
			continue;

		uint64_t identity = *mem.Read<uint64_t>(ent + 0x10);

		const char* name = GetEntityName(mem,identity);
		//printf_s("%p %s\n", ent, name);

		if (strcmp(name, "env_fog_controller") == 0) {
			uint64_t ent = *mem.Read<uint64_t>(identity);

			uintptr_t fogparams = ent + 0x5E0;
			bool enabled = *mem.Read<uint8_t>(fogparams + 0x64);

			// !!!! РАБОТАЕТ!!!
			if (isfarz) {
				mem.Write<float>(fogparams + 0x2c, 18000.f);
			}
			else {
				mem.Write(fogparams + 0x64, !enabled);
			}
		}

		free((void*)name);
	}
}

//void ccvars(blackbone::ProcessMemory& mem, uint64_t ICVar) {
//	struct ConCommand {
//	public:
//		char* m_name{}; // 0x0
//		char* m_description{}; // 0x8
//		std::int32_t m_flags{}; // 0x10
//	private:
//		std::int32_t unk0{}; // 0x14
//	public:
//		void* m_member_accessor_ptr{}; // 0x18
//		void* m_callback{}; // 0x20
//	private:
//		void* unk1{}; // 0x28
//		void* unk2{}; // 0x30
//	};
//
//	uint16_t commands_size = *mem.Read<uint16_t>(ICVar + 0xF0);
//
//	for (int i = 0; i < commands_size; i++) {
//		/*ConCommand current_cvar;
//		mem.Read(*mem.Read<uint64_t>((ICVar + 0xd8) + (i * sizeof(ConCommand))), sizeof(ConCommand), &current_cvar);*/
//
//		char buf[256];
//		auto a = *mem.Read<uint64_t>(*mem.Read<uint64_t>((ICVar + 0xd8))) + (i * sizeof(ConCommand));
//		mem.Read(a, 256, buf);
//
//		std::cout << buf << '\n' << '\n';
//	}
//}

void change_weather(blackbone::ProcessMemory& mem, blackbone::ModuleDataPtr& client, uint64_t weather) {
	BYTE GetWeatherType_enter_bytes = *mem.Read<BYTE>(weather);
	BYTE BytePatch[6] = { 0xB8,0xcc, 0x00,0x00,0x00, // mov eax, ...
									  0xC3 };		 // ret
	BytePatch[1] = (BYTE)req_action(2);

	if (BytePatch[1] != 0) {
		mem.Write(weather, sizeof(BytePatch), BytePatch);

		//GetLocalPlayer(mem, client);
	}
	else if (GetWeatherType_enter_bytes == 0xb8) {
		// push rbx
		// sub rsp, 20
		mem.Write(weather, 6, "\x40\x53\x48\x83\xEC\x20");
	}
}

void process(blackbone::Process& dota_proc,
	blackbone::ProcessMemory& DOTAMemory) {
	static auto client = dota_proc.modules().GetModule(L"client.dll");

	auto DOTACamera = FindCamera(dota_proc);

	// EB, 0E | jmp client.7FF9A7BBCFFA
	constexpr const char* bytePatch = "\xEB\x0E";
	// 75, 0E | jne client.7FF9A7BBCFFA
	constexpr const char* byteRestore = "\x75\x0E";

	static uint64_t particles_addr = 0;
	static uint64_t emoticon_addr = 0;
	static uint64_t icvar_addr = 0;
	static uint64_t weather_addr = 0;
	uint64_t insn_bytes = 0, insn_bytes2 = 0, insn_bytes3 = 0;
	static uint64_t ppGameEntitySystem = 0;

	static bool first = true;
	if (first) {
		std::vector<blackbone::ptr_t> search_result, search_result2;

		blackbone::PatternSearch and_al_01{ "\x4C\x8B\xDC\x55\x56\x41\x54" };
		and_al_01.SearchRemote(
			dota_proc, 0xCC,
			dota_proc.modules().GetModule(L"particles.dll").get()->baseAddress,
			dota_proc.modules().GetModule(L"particles.dll").get()->size,
			search_result, 1
		);

		if (search_result.empty()) {
			std::cout << "cant find particles pattern\n";
			getchar();
			exit(1);
		}

		particles_addr = search_result.front() + 0x2c;

		search_result.clear();

		blackbone::PatternSearch game_ent_sys{ "\x48\x8B\x0D\xCC\xCC\xCC\xCC\x33\xD2\xE8\xCC\xCC\xCC\xCC" };
		game_ent_sys.SearchRemote(dota_proc, 0xcc, client->baseAddress, client->size, search_result, 1);

		ppGameEntitySystem = DOTAMemory.GetAbsoluteAddress(search_result.front());

		search_result.clear();

		//56 57 48 83 EC ? 33 FF 8B F2
		blackbone::PatternSearch is_emoticon_unlocked{ "\x56\x57\x48\x83\xEC\xCC\x33\xFF\x8B\xF2" };
		is_emoticon_unlocked.SearchRemote(dota_proc, 0xcc, client->baseAddress, client->size, search_result, 1);

		emoticon_addr = search_result.front() - 0x6;

		search_result.clear();
		//48 8B 0D ? ? ? ? 4C 8D 44 24 ? 4C 8B 0D ? ? ? ? 48 8D 94 24 - ICVar
		//\xE9\x00\x00\x00\x00\x33\xC0\x48\x83\xC4\x00\x5B\xC3\xCC\xCC\xCC\xCC\xCC\xCC\x33\xC0 xadadxxxxx?xxxxxxxxxx - weather
		blackbone::PatternSearch weather_pattern{ "\x48\x8B\xD9\xBA\xad\xad\xad\xad\x48\x8D\x0D\xad\xad\xad\xad\xE8\xad\xad\xad\xad\x48\x85\xC0\x75\xad\x48\x8B\x05\xad\xad\xad\xad\x48\x8B\x40\xad\x83\x38\xad\x74\xad\xBA\xad\xad\xad\xad\x48\x8D\x0D\xad\xad\xad\xad\xE8\xad\xad\xad\xad\x48\x85\xC0\x75\xad\x48\x8B\x05\xad\xad\xad\xad\x48\x8B\x40\xad\x8B\x00" };
		weather_pattern.SearchRemote(dota_proc, 0xad, client->baseAddress,
			client->size, search_result, 1);

		weather_addr = search_result.front() - 0x6;

		first = false;
	}

	if (DOTACamera.IsValid()) {
		const auto act = req_action(0);

		switch (act) {
		case 1:
			DOTACamera.SetDistance(req_action(1));
			break;
		case 2:
			toggle_fog_farz(DOTAMemory, ppGameEntitySystem, true);
			break;
		case 3:
			toggle_fog_farz(DOTAMemory, ppGameEntitySystem);
			//DOTACamera.SetFarZ(18000.f);

			//DOTACamera.ToggleFog();
			break;
		case 4:
			insn_bytes2 = DOTAMemory.Read<USHORT>(particles_addr).result();
			if (insn_bytes2 == 0x08b41) {
				DOTAMemory.Write(particles_addr, 3, "\xb2\x01\x90");
			}
			else if (insn_bytes2 == 0x01b2) {
				DOTAMemory.Write(particles_addr, 3, "\x41\x8B\xED");
			}
			else {
				std::cout << "[unknown bytes at " << std::hex << particles_addr << '\n';
				std::system("pause");
				exit(1);
			}
			break;
		case 5:
			insn_bytes3 = *DOTAMemory.Read<uint64_t>(emoticon_addr);
			//std::cout << insn_bytes3 << '\n';
			if (insn_bytes3 == 0x5756c390909001b0) {
				DOTAMemory.Write(emoticon_addr, 6, "\x48\x89\x5C\x24\x08\x55");
			}
			else if (insn_bytes3 == 0x57565508245c8948) {
				DOTAMemory.Write(emoticon_addr, 6, "\xb0\x01\x90\x90\x90\xc3");
			}
			else {
				std::cout << "[unknown bytes at " << std::hex << emoticon_addr << '\n';
				std::system("pause");
				exit(1);
			}

			break;
		case 6:
			change_weather(DOTAMemory, client, weather_addr);
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
		std::cout << dye::green("Attached to dota2.exe\n") << std::endl;
		while (1)
			process(dota, dota.memory());
	}
	else {
		std::cout << dye::black_on_red("dota2.exe not found or not client.dll")
			<< std::endl;
		std::system("pause");
		exit(1);
	}
}
