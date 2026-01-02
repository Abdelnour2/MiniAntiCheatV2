#pragma once

struct GameProcessIdInput {
	ULONG_PTR processID;
};

struct MiniAntiCheatOutput {
	bool isSafeToStart;
};

#define CHECK_BLACKLIST_TO_START CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define GET_GAME_PROCESS_ID CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DISABLE_SHIELD CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define PROCESS_TERMINATE         (0x0001)  
#define PROCESS_VM_OPERATION      (0x0008)  
#define PROCESS_VM_READ           (0x0010)  
#define PROCESS_VM_WRITE          (0x0020)