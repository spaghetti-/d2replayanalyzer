#ifndef DEMODUMP_H
#define DEMODUMP_H

#include "generated_proto/netmessages.pb.h"
#include "demofile.h"
#include <vector>
#include <iostream>

class CDemoFileDump
{
public:
	CDemoFileDump() : m_nFrameNumber( 0 ) {}
	~CDemoFileDump() {}

	bool Open( const char *filename ); 
	void Dump();
	void Output();

public:
	void DumpDemoPacket( const std::string& buf );
	void DumpUserMessage( const void *parseBuffer, int BufferSize );
	void PrintDemoHeader( EDemoCommands DemoCommand, int tick, int size, int uncompressed_size );
	void MsgPrintf( const ::google::protobuf::Message& msg, int size, const char *fmt, ... );

public:
	CDemoFile m_demofile;
	CSVCMsg_GameEventList m_GameEventList;
	std::multimap<short, short> clog;
	std::map<int, std::string> cname;
	std::map<std::string, std::string> pname;
	int m_nFrameNumber;
public:
	struct hero{
		int id;
		std::string name;
		std::string player;
		int kills;
		int deaths;
		int lh;
	};
};
#endif