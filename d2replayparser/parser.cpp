#include <stdarg.h>
#include "demofile.h"
#include "demodump.h"
#include <algorithm>
#include <iostream>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/descriptor.pb.h"

#include "generated_proto/usermessages.pb.h"
#include "generated_proto/ai_activity.pb.h"
#include "generated_proto/dota_modifiers.pb.h"
#include "generated_proto/dota_commonmessages.pb.h"
#include "generated_proto/dota_usermessages.pb.h"


bool CDemoFileDump::Open(const char *filename)
{
	if (!m_demofile.Open(filename)){
		fprintf(stderr, "Couldn't open '%s'\n", filename);
		return false;
	}
	return true;
}

static std::string GetNetMsgName(int Cmd)
{
	if(NET_Messages_IsValid(Cmd)){
		return NET_Messages_Name((NET_Messages)Cmd);
	}
	else if(SVC_Messages_IsValid(Cmd)){
		return SVC_Messages_Name((SVC_Messages)Cmd);
	}

	assert(0);
	return "NETMSG_???";
}

template <class T, int msgType>
void PrintNetMessage(CDemoFileDump& Demo, const void *parseBuffer, int BufferSize)
{
	T msg;

	if(msg.ParseFromArray(parseBuffer, BufferSize))
	{
		if(msgType == svc_GameEventList)
		{
			Demo.m_GameEventList.CopyFrom(msg);
		}
	}
}

template <>
void PrintNetMessage< CSVCMsg_GameEvent, svc_GameEvent >( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
	CSVCMsg_GameEvent msg;
	if(msg.ParseFromArray(parseBuffer, BufferSize)){
	int ndesc = 0;
	for(ndesc; ndesc < Demo.m_GameEventList.descriptors().size(); ndesc++){
		const CSVCMsg_GameEventList::descriptor_t& Descriptor = Demo.m_GameEventList.descriptors(ndesc);
		if (Descriptor.eventid() == msg.eventid())
			break;
	}
	if(msg.eventid() == 190){
		const CSVCMsg_GameEventList::descriptor_t& Descriptor = Demo.m_GameEventList.descriptors(ndesc);
		int n = msg.keys().size();
		for(int i = 0; i < n; i++){
			const CSVCMsg_GameEvent::key_t &keyvalue = msg.keys(i);
			const CSVCMsg_GameEventList::key_t &desc = Descriptor.keys(i);
			if(!strcmp(desc.name().c_str(), "type") && keyvalue.val_byte() != 4)
				break;
			//printf("%s : %d\n", desc.name().c_str(), keyvalue.has_val_byte() ? keyvalue.val_byte() : keyvalue.val_short());
			short attacker, target;
			if(!strcmp(desc.name().c_str(), "targetname"))
				attacker = keyvalue.val_short();
			if(!strcmp(desc.name().c_str(), "attackername"))
				target = keyvalue.val_short();
			if(!strcmp(desc.name().c_str(), "targetillusion"))
				if(!keyvalue.val_bool()){
					Demo.clog.insert(std::pair<short, short>(attacker, target));
					attacker = target = 0;
				}
			}
		}
	}
}

static void ParseStringTable(CDemoFileDump &d, const CDemoStringTables &strTable)
{
	for(int i = 0; i < strTable.tables().size(); i++){
		const CDemoStringTables::table_t &Table = strTable.tables(i);
		if(!strcmp(Table.table_name().c_str(), "CombatLogNames")){
			for(int item = 0; item < Table.items().size(); item++){
				const CDemoStringTables::items_t &Item = Table.items(item);
				d.cname.insert(std::pair<int, std::string>(item, Item.str()));
			}
		}
	}
}


void CDemoFileDump::DumpDemoPacket( const std::string& buf )
{
	size_t index = 0;

	while( index < buf.size() )
	{
		int Cmd = ReadVarInt32( buf, index );
		uint32 Size = ReadVarInt32( buf, index );

		if( index + Size > buf.size() )
		{
			const std::string& strName = GetNetMsgName( Cmd );
		}

		switch( Cmd )
		{
#define HANDLE_SvcMsg( _x )		case svc_ ## _x: PrintNetMessage< CSVCMsg_ ## _x, svc_ ## _x >( *this, &buf[ index ], Size ); break
			HANDLE_SvcMsg( GameEvent );
			HANDLE_SvcMsg( GameEventList );
#undef HANDLE_SvcMsg
		}
		index += Size;
	}
}

template <class DEMCLASS>
void handleDemoMessage(CDemoFileDump &d, bool isc, int tick, int &size, int &usize)
{
	DEMCLASS msg;
	d.m_demofile.ReadMessage(&msg, isc, &size, &usize);
}

template <>
void handleDemoMessage<CDemoFileInfo_t>(CDemoFileDump &d, bool isc, int tick, int &size, int &usize)
{
	int mode, winner;
	uint32 matchid;
	float duration;
	CDemoFileInfo_t Msg;
	if(d.m_demofile.ReadMessage(&Msg, isc, &tick, &usize)){
		matchid = Msg.game_info().dota().match_id();
		mode = Msg.game_info().dota().game_mode();
		winner = Msg.game_info().dota().game_winner();
		duration = Msg.playback_time();
		printf("Match-id: %d\nMode: %d\nWinner: %d\nDuration: %f\n", matchid, mode, winner, duration);
		for(int i = 0; i < 10; i++){
			//printf("%s (%s) 0/0 0 i1 i2 i3 i4 i5 i6\n", Msg.game_info().dota().player_info(i).player_name().c_str(), Msg.game_info().dota().player_info(i).hero_name().c_str());
			d.pname.insert(std::pair<std::string, std::string>(Msg.game_info().dota().player_info(i).hero_name(), Msg.game_info().dota().player_info(i).player_name()));
		}

	}
}

void CDemoFileDump::Dump()
{
	bool stop = false;
	for(m_nFrameNumber = 0; !stop; m_nFrameNumber++){
		int tick = 0;
		int size = 0;
		bool isc;
		int usize = 0;
		if(m_demofile.IsDone())
			break;
		EDemoCommands cmd = m_demofile.ReadMessageType(&tick, &isc);
		switch(cmd){
#define handle_dem(_x) case DEM_ ## _x: handleDemoMessage<CDemo ## _x ## _t>(*this, isc, tick, size, usize); break;
			handle_dem(FileHeader);
			handle_dem(FileInfo);
			handle_dem(Stop);
			handle_dem(SyncTick);
			handle_dem(ConsoleCmd);
			handle_dem(SendTables);
			handle_dem(ClassInfo);
			handle_dem(StringTables);
			handle_dem(UserCmd);
			handle_dem(CustomDataCallbacks);
			handle_dem(CustomData);
#undef handle_dem
			case DEM_FullPacket:
				{
					CDemoFullPacket_t fp;
					m_demofile.ReadMessage(&fp, isc, &size, &usize);
					ParseStringTable(*this, fp.string_table());
					//DumpDemoStringTable(*this, fp.string_table());
					DumpDemoPacket(fp.packet().data());
				}
				break;
			case DEM_Packet:
			case DEM_SignonPacket:
				{
					CDemoPacket_t p;
					m_demofile.ReadMessage(&p, isc, &size, &usize);
					DumpDemoPacket(p.data());
				}
				break;
			default:
			case DEM_Error:
				stop = true;
				break;

		}
	}
}

struct ValueComp{
	const int v;
	ValueComp(int v) : v (v) {}
	bool operator()(CDemoFileDump::hero i) const{
		return i.id == v;
	}
};

void CDemoFileDump::Output()
{
	std::map<short, short>::iterator it_clog;
	std::vector<hero> herolist;
	std::map<int, std::string>::iterator it_cname;
	for(it_cname = cname.begin(); it_cname != cname.end(); it_cname++){
		if(!strncmp(it_cname->second.c_str(), "npc_dota_hero", strlen("npc_dota_hero"))){
			hero t;
			t.id = it_cname->first;
			t.name = it_cname->second;
			t.deaths = 0;
			t.kills = 0;
			t.lh = 0;
			herolist.push_back(t);
		}
	}
	for(it_clog = clog.begin(); it_clog != clog.end(); it_clog++){
		std::vector<hero>::iterator t, x;
		t = std::find_if(herolist.begin(), herolist.end(), ValueComp((int) it_clog->second));
		x = std::find_if(herolist.begin(), herolist.end(), ValueComp((int) it_clog->first));
		if(t != herolist.end() && x != herolist.end()){
			t->kills++;
			x->deaths++;
			t->lh--;
			x->lh--;
		}
		if(t != herolist.end()){
			t->lh++;
		}
	}
	printf("playername (hero) - k/d cs\n");
	for(int i = 0; i < herolist.size(); i++){
		printf("%s (%s) - %d/%d %d\n", pname.at(herolist.at(i).name.c_str()).c_str(), herolist.at(i).name.c_str(), herolist.at(i).kills, herolist.at(i).deaths, herolist.at(i).lh);
		//std::cout << herolist.at(i).name << ": " << herolist.at(i).kills << "/" << herolist.at(i).deaths << std::endl;
	}

}