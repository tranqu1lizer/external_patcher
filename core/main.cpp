#include "dota_sdk.hpp"

int main( )
{
	blackbone::Process dota;

	if ( NT_SUCCESS( dota.Attach( L"dota2.exe" ) ) ) {
		auto pClientModule = dota.modules( ).GetModule( L"client.dll" );
		auto& pDOTAMemory = dota.memory( );

		if ( pClientModule ) {
			std::cout << "Attached to dota2.exe " << std::endl;
			std::vector<blackbone::ptr_t> search_result;
			blackbone::PatternSearch aDOTACameraInit_Pattern{ "\x48\x83\xEC\x38\xE8\xCC\xCC\xCC\xCC\x48\x85\xC0\x74\x4D" };

			while ( 1 ) {
				std::cout << "PID: " << std::dec << dota.pid( ) << std::endl << "client.dll base: " << (void*)pClientModule.get( )->baseAddress << std::endl;
				aDOTACameraInit_Pattern.SearchRemote( dota, 0xCC, pClientModule.get( )->baseAddress, pClientModule.get( )->size, search_result, 1 );
				const auto aGetCamera = search_result[0];

				if ( aGetCamera /* && pDOTAParticleManager */ ) {
					std::cout << "=================================\n[0] Change camera distance\n[1] Patch ZFar\n[2] Toggle Fog\n=> ";
					int act;
					std::cin >> act;
					blackbone::RemoteFunction<CDOTACamera__Init> pFN( dota, aGetCamera );
					if ( auto result = pFN.Call( ); result.success( ) && result.result( ) ) {

						CDOTA_Camera DOTACamera( &pDOTAMemory, result.result( ) );

						if ( act == 0 ) {
							float dist;
							std::system( "cls" );
							std::cout << "=> ";
							std::cin >> dist;
							DOTACamera.SetDistance( dist );
						}
						else if ( act == 1 ) {
							DOTACamera.ToggleMaxZFar( );
						}
						else if ( act == 2 ) {
							DOTACamera.ToggleFog( );
						}
						else exit( 0 );
					}
					else {
						std::cout << "CDOTACamera or SetRenderingEnabled not found" << std::endl;
						std::system( "pause" );
						exit( 1 );
					}
				}
				std::system( "cls" );
			}
		}
	}
	else {
		std::cout << "dota2.exe not found" << std::endl;
		std::system( "pause" );
		exit( 1 );
	}
}
