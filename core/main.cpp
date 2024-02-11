#include "dota_sdk.hpp"

[[nodiscard]] inline int req_action( byte w ) {
	if ( w == 0 ) {
		int act1;

		std::cout << "=================================\n[0] Change camera distance\n[1] Patch ZFar\n[2] Toggle Fog\n=> ";
		std::cin >> act1;
		return act1;
	}
	else {
		int dist;
		std::system( "cls" );
		std::cout << "=> ";
		std::cin >> dist;

		return dist;
	}
}

void process( blackbone::Process& dota_proc, blackbone::ProcessMemory& pDOTAMemory ) {
	std::cout << "PID: " << std::dec << dota_proc.pid( ) << std::endl << "client.dll base: " << (void*)dota_proc.modules( ).GetModule( L"client.dll" ).get( )->baseAddress << std::endl;

	auto DOTACamera = FindCamera( dota_proc );

	if ( DOTACamera.IsValid( ) ) {
		const auto act = req_action( 0 );

		switch ( act )
		{
		case 0:
			DOTACamera.SetDistance( req_action( 1 ) );
			break;
		case 1:
			DOTACamera.ToggleMaxZFar( );
			break;
		case 2:
			DOTACamera.ToggleFog( );
			break;
		default:
			exit( 0 );
		}
	}
	std::system( "cls" );
}

int main( ) {
	blackbone::Process dota;

	if ( NT_SUCCESS( dota.Attach( L"dota2.exe" ) ) && dota.modules( ).GetModule( L"client.dll" ) ) {
		std::cout << "Attached to dota2.exe " << std::endl;
		while ( 1 ) process( dota, dota.memory( ) );
	}
	else {
		std::cout << "dota2.exe not found" << std::endl;
		std::system( "pause" );
		exit( 1 );
	}
}
