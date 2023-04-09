#include <iostream>
#include <format>
#include <xorstr/xorstr.hpp>
#include <BlackBone/Asm/AsmFactory.h>
#include <BlackBone/Asm/AsmHelper64.h>
#include <BlackBone/Asm/IAsmHelper.h>
#include <BlackBone/Process/Process.h>
#include <BlackBone/Process/RPC/RemoteHook.h>
#include <BlackBone/Process/RPC/RemoteFunction.hpp>
#include <BlackBone/Patterns/PatternSearch.h>

typedef std::uintptr_t( __fastcall* CDOTACamera__Init )( );
typedef void( __fastcall* SetRenderingEnabled )( std::uintptr_t CParticleCollectionPtr, bool render );

class DefClass {
public:
	blackbone::ProcessMemory* memory;
	std::uintptr_t baseAddr;

	std::uintptr_t GetVF( unsigned short idx ) noexcept {
		const auto VMTAddr = memory->Read<std::uintptr_t>( baseAddr ).result( );

		return memory->Read<std::uintptr_t>( VMTAddr + idx * 8 ).result( );
	}
};

class CDOTA_ParticleManager : DefClass {
public:
	CDOTA_ParticleManager( ) {
		memory = nullptr;
		baseAddr = NULL;
	}

	CDOTA_ParticleManager( blackbone::ProcessMemory* memory, std::uintptr_t baseAddr ) {
		this->memory = memory;
		this->baseAddr = baseAddr;
	}

	class NewParticleEffect : DefClass {
	public:
		NewParticleEffect( ) {
			memory = nullptr;
			baseAddr = NULL;
		}
		NewParticleEffect( blackbone::ProcessMemory* memory, std::uintptr_t baseAddr ) {
			this->memory = memory;
			this->baseAddr = baseAddr;
		}
		DefClass GetParticleCollection( ) {
			auto partCollection = memory->Read<std::uintptr_t>( baseAddr + 0x20 ).result( );
			auto res = DefClass( memory, partCollection );
			if ( !baseAddr || !partCollection ) return DefClass( 0, 0 );
			return res;
		}
	};

	class ParticleListItem : DefClass {
	public:
		ParticleListItem( ) {
			memory = nullptr;
			baseAddr = NULL;
		}
		ParticleListItem( blackbone::ProcessMemory* memory, std::uintptr_t baseAddr ) {
			this->memory = memory;
			this->baseAddr = baseAddr;
		}
		CDOTA_ParticleManager::NewParticleEffect GetNewParticleEffect( ) {
			auto newPartEffect = memory->Read<std::uintptr_t>( baseAddr + 0x10 ).result( );
			if ( !newPartEffect || !baseAddr ) return CDOTA_ParticleManager::NewParticleEffect( 0, 0 );
			return CDOTA_ParticleManager::NewParticleEffect( memory, newPartEffect );
		}
	};


	std::vector<CDOTA_ParticleManager::ParticleListItem> GetParticleLists( ) {
		std::vector<ParticleListItem> list;
		auto particlesBase = memory->Read<std::uintptr_t>( baseAddr + 0x88 ).result( );
		const auto pCount = this->GetParticleCount( ) * 8;
		for ( int idx = 0x0; idx < pCount; idx += 0x8 ) {
			CDOTA_ParticleManager::ParticleListItem thisEffect( memory, memory->Read<std::uintptr_t>( particlesBase + idx ).result( ));
			list.push_back( thisEffect );
		}

		return list;
	}

	int GetParticleCount( ) {
		return memory->Read<int>( baseAddr + 0x80 ).result( );
	}
};

class CDOTA_Camera : DefClass {
public:
	CDOTA_Camera( ) {
		memory = nullptr;
		baseAddr = NULL;
	}

	CDOTA_Camera( blackbone::ProcessMemory* memory, std::uintptr_t baseAddr ) {
		this->memory = memory;
		this->baseAddr = baseAddr;
	}

	void SetDistance( float distance ) noexcept { // 0x270
		memory->Write<float>( baseAddr + 0x270, distance );
	}

	void SetFOWAmount( float amount ) // 0x70
	{
		memory->Write<float>( baseAddr + 0x70, amount );
	}

	auto GetDistance( ) noexcept {
		return memory->Read<float>( baseAddr + 0x270 ).result( );
	}

	auto GetFOWAmount( )
	{
		return memory->Read<float>( baseAddr + 0x70 ).result( );
	}

	void ToggleFog( ) {
		const auto aGetFog = this->GetVF( 18 );
		const auto instructionBytes = memory->Read<uintptr_t>( aGetFog ).result( );

		if ( instructionBytes == 0x83485708245c8948 ) { // not patched

			// 0x0F, 0x57, 0xC0 | xorps xmm0, xmm0
			// 0xC3				| ret
			constexpr const char* bytePatch = "\x0F\x57\xC0\xC3";
			memory->Write( aGetFog, 4, bytePatch );
			// std::cout << "Fog instructions patched" << std::endl;
		}
		else if ( instructionBytes == 0x83485708c3c0570f ) { // already patched

			// 0x48, 0x89, 0x5C, 0x24, 0x08 | mov qword ptr ss:[rsp+8], rbx
			// 0x57							| push rdi
			constexpr const char* byteRestore = "\x48\x89\x5C\x24\x08\x57";
			memory->Write( aGetFog, 6, byteRestore );
			// std::cout << "Fog instructions restored" << std::endl;
		}
		else {
			std::cout << "Error, unknown fog instructions: " << instructionBytes << std::endl;
			std::system( "pause" );
			exit( 1 );
		}
	}

	void ToggleMaxZFar( ) {
		const auto aGetZFar = this->GetVF( 19 );
		const auto instructionBytes = memory->Read<uintptr_t>( aGetZFar ).result( );

		if ( instructionBytes == 0x83485708245c8948 ) { // not patched

			// 0xB8, 0x50, 0x46, 0x00, 0x00	| mov eax, 18000
			// 0xF3, 0x0F, 0x2A, 0xC0		| cvtsi2ss xmm0, eax
			// 0xC3							| ret
			constexpr const char* bytePatch = "\xB8\x50\x46\x00\x00\xF3\x0F\x2A\xC0\xC3";
			memory->Write( aGetZFar, 10, bytePatch );
			//std::cout << "ZFar instructions patched" << std::endl;
		}
		else if ( instructionBytes == 0x2a0ff300004650b8 ) { // already patched

			// 0x48, 0x89, 0x5C, 0x24, 0x08 | mov qword ptr ss:[rsp+8], rbx
			// 0x57							| push rdi
			// 0x48, 0x83, 0xEC, 0x40       | sub rsp, 40
			constexpr const char* byteRestore = "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x40";
			memory->Write( aGetZFar, 10, byteRestore );
			//std::cout << "ZFar instructions restored" << std::endl;
		}
	}
};