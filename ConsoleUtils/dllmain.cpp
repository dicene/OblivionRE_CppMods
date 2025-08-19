#include "Mod/CppUserModBase.hpp"
#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Exceptions/AVehHook.hpp>
#include "UE4SSProgram.hpp"
#include "Unreal/AActor.hpp"
#include "Unreal/UClass.hpp"
#include "Unreal/UFunction.hpp"
#include <SigScanner/SinglePassSigScanner.hpp>

using namespace RC;
using namespace RC::Unreal;

HMODULE BaseModule;
uint64_t BaseModuleAdd;

PLH::x64Detour* ConsolePrint_Detour;
PLH::x64Detour* EngineTick_Detour;
FNativeFuncPtr funcPtr;

const wchar_t* GetWC(const char* c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);

    return wc;
}

auto OutputBuffer = std::string();

bool CapturingOutput = false;

size_t GameEngineThread;

struct ConsoleCommandString
{
    const char* text;
    void* gap;
    uint64_t num;
    uint64_t max;
};

typedef void ExecuteConsoleCommandFuncType(uint64_t, uint64_t, ConsoleCommandString&);
ExecuteConsoleCommandFuncType* ExecuteConsoleCommand;
typedef int IsOblivionInitializedFuncType();
IsOblivionInitializedFuncType* IsOblivionInitialized;

uint64_t(*EngineTick_Original)(uint64, uint64, uint64);
uint64_t(*ConsolePrint_Original)(const char* fmt...);

uint64_t EngineTick_Replacement(uint64 rcx, uint64 rdx, uint64 r8)
{
    GameEngineThread = std::hash<std::thread::id>{}(std::this_thread::get_id());
    Output::send(STR("GameEngineThread Hash: {:#08x}\n"), GameEngineThread);
    auto result = EngineTick_Original(rcx, rdx, r8);
    EngineTick_Detour->unHook();
    return result;
}

uint64_t ConsolePrint_Replacement(const char* fmt...)
{
    va_list args;
    va_start(args, fmt);

    char tempBuff[2048];
    memset(tempBuff, 0, sizeof tempBuff);
    auto done = vsprintf_s(tempBuff, fmt, args);

    va_end(args);
    //Output::send(STR("Printing to console: ({})\n"), GetWC(tempBuff));

    //Output::send(STR("Capturing output...\n"));
    OutputBuffer.append(tempBuff);

    if (CapturingOutput) {
        return 0;
    }

    return ConsolePrint_Original(tempBuff);
}

template <class... Args>
void PrintToConsole(const char* fmt, Args... args)
{
    using func_t = void(*)(const char*, ...);
    auto func = reinterpret_cast<func_t>(BaseModuleAdd + 0x656A7A0);
    return func(fmt, args...);
}

void ExecuteCommand(const char* commandText)
{
    //Output::send(STR("Firing a console command ({}).\n"), GetWC(commandText));

    ConsoleCommandString* customString = new (FMemory::Malloc(0x20, 0x10)) ConsoleCommandString();
    //Output::send(STR("Constructing a string of length {} at {:#08x}: '{}'\n"), strlen(commandText) + 1, (uint64_t)customString, GetWC(commandText));
    auto text = (char*)FMemory::Malloc(strlen(commandText) + 1, 0x10);
    strcpy(text, commandText);
    customString->text = text;
    //Output::send(STR("Copied text: {}\n"), GetWC(customString->text));
    customString->num = strlen(customString->text) + 1;
    customString->max = (strlen(customString->text) + 1) | 0xf;

    if (customString->max < 0x10) {
        //Output::send(STR("Padded string to 0x10 length from {:#02x} length\n"), customString->max);
        customString->max = 0x10;
    }

    CapturingOutput = true;
    OutputBuffer.clear();
    ExecuteConsoleCommand(0, 0, *customString);
    CapturingOutput = false;
}

size_t GetCurrentThreadHash()
{
    auto result = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return result;
}

class ConsoleUtils : public RC::CppUserModBase
{
public:
    ConsoleUtils() : CppUserModBase()
    {
        ModName = STR("ConsoleUtils");
        ModVersion = STR("1.0");
        ModDescription = STR("A set of tools to enable calling and reading the response from Oblivion Remastered's custom Console Commands.");
        ModAuthors = STR("Dicene");
    }
    ~ConsoleUtils() override = default;

    auto on_unreal_init() -> void override
    {
        BaseModule = GetModuleHandleW(NULL);
        BaseModuleAdd = (uint64_t)BaseModule;

		const SignatureContainer executeConsoleCommandSig{
			{{"48 89 5C 24 08 48 89 74 24 20 57 48 81 EC D0 00 00 00 48 8B ?? ?? ?? ?? ?? 48 33 C4 48 89 ?? ?? ?? ?? ?? ?? 49 8b d8"}},
			[&](const SignatureContainer& self) {
			    //ChangeAnime_Detour = new PLH::x64Detour(reinterpret_cast<uint64_t>(self.get_match_address()), reinterpret_cast<uint64_t>(&ChangeAnime_Hook), reinterpret_cast<uint64_t*>(&ChangeAnime_Orig));
			    //ChangeAnime_Detour->hook();
			    if (self.get_did_succeed())
                {
				    Output::send(STR("Found ExecuteConsoleCommand via scan at {:#08x}"), self.get_match_address());
			    }
                else
                {
                    Output::send(STR("Failed to find ExecuteConsoleCommand via scan"));
                }
                return true;
            },
            [](SignatureContainer& self) { },
		};

        const SignatureContainer vOblivionUEPairingGateIsInitializedSig{
            {{"48 8b 74 24 50 48 8b c3 48 89 1d ?? ?? ?? ?? 48 8b 5c 24 48 48 83 c4 38"}},
            [&](const SignatureContainer& self) {
                //SetAnimeFrame_Detour = new PLH::x64Detour(reinterpret_cast<uint64_t>(self.get_match_address()), reinterpret_cast<uint64_t>(&SetAnimeFrame_Hook), reinterpret_cast<uint64_t*>(&SetAnimeFrame_Orig));
                //SetAnimeFrame_Detour->hook();
                if (self.get_did_succeed()) {
                    Output::send(STR("Found VOblivionUEPairingGateIsInitialized via scan at {:#08x}"), self.get_match_address());
                }
                else
                {
                    Output::send(STR("Failed to find VOblivionUEPairingGateIsInitialized via scan"));
                }
                return true;
            },
            [](SignatureContainer& self) { },
        };

        const SignatureContainer consolePrintSig{
            {{"48 89 ?? ?? ?? 48 89 ?? ?? ?? 4c 89 ?? ?? ?? 4c 89 ?? ?? ?? 48 83 ec 28 80 3d ?? ?? ?? ?? ?? 74 26"}},
            [&](const SignatureContainer& self) {
                //SetAnimeFrame_Detour = new PLH::x64Detour(reinterpret_cast<uint64_t>(self.get_match_address()), reinterpret_cast<uint64_t>(&SetAnimeFrame_Hook), reinterpret_cast<uint64_t*>(&SetAnimeFrame_Orig));
                //SetAnimeFrame_Detour->hook();
                if (self.get_did_succeed()) {
                    Output::send(STR("Found ConsolePrint via scan at {:#08x}"), self.get_match_address());
                }
                else
                {
                    Output::send(STR("Failed to find ConsolePrint via scan"));
                }
                return true;
            },
            [](SignatureContainer& self) {},
        };

        const SignatureContainer engineTickSig{
            {{"44 88 44 24 18 53 57 41 54 41 56 48 81 ec ?? ?? ?? ??"}},
            [&](const SignatureContainer& self) {
                //SetAnimeFrame_Detour = new PLH::x64Detour(reinterpret_cast<uint64_t>(self.get_match_address()), reinterpret_cast<uint64_t>(&SetAnimeFrame_Hook), reinterpret_cast<uint64_t*>(&SetAnimeFrame_Orig));
                //SetAnimeFrame_Detour->hook();
                if (self.get_did_succeed()) {
                    Output::send(STR("Found EngineTick via scan at {:#08x}"), self.get_match_address());
                }
                else
                {
                    Output::send(STR("Failed to find EngineTick via scan"));
                }
                return true;
            },
            [](SignatureContainer& self) {},
        };

        std::vector<SignatureContainer> signature_containers;
        signature_containers.push_back(executeConsoleCommandSig);
        signature_containers.push_back(vOblivionUEPairingGateIsInitializedSig);
        signature_containers.push_back(consolePrintSig);
        signature_containers.push_back(engineTickSig);

        SinglePassScanner::SignatureContainerMap signature_containers_map;
        signature_containers_map.emplace(ScanTarget::MainExe, signature_containers);

        SinglePassScanner::start_scan(signature_containers_map);

        ExecuteConsoleCommand = (ExecuteConsoleCommandFuncType*)(BaseModuleAdd + 0x6555280); // 48 89 5C 24 08 48 89 74 24 20 57 48 81 EC D0 00 00 00 48 8B ?? ?? ?? ?? ?? 48 33 C4 48 89 ?? ?? ?? ?? ?? ?? 49 8b d8
        IsOblivionInitialized = (IsOblivionInitializedFuncType*)(BaseModuleAdd + 0x4595EE0); // Var this func points to, (aob+0xb) + (aob + 0xf) -> 48 8b 74 24 50 48 8b c3 48 89 1d ?? ?? ?? ?? 48 8b 5c 24 48 48 83 c4 38

        ConsolePrint_Detour = new PLH::x64Detour(
            BaseModuleAdd + 0x656A7A0, // 48 89 ?? ?? ?? 48 89 ?? ?? ?? 4c 89 ?? ?? ?? 4c 89 ?? ?? ?? 48 83 ec 28 80 3d ?? ?? ?? ?? ?? 74 26
            reinterpret_cast<uint64_t>(&ConsolePrint_Replacement),
            reinterpret_cast<uint64_t*>(&ConsolePrint_Original));
        ConsolePrint_Detour->hook();

        EngineTick_Detour = new PLH::x64Detour(
            BaseModuleAdd + 0x31C4D60, // 44 88 44 24 18 53 57 41 54 41 56 48 81 ec ?? ?? ?? ??
            reinterpret_cast<uint64_t>(&EngineTick_Replacement),
            reinterpret_cast<uint64_t*>(&EngineTick_Original));
        EngineTick_Detour->hook();
    }

    auto on_lua_start(StringViewType mod_name, LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua, std::vector<LuaMadeSimple::Lua*>& hook_luas) -> void override
    {
        lua.register_function("GetGameThread", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.set_integer(GameEngineThread);
            lua.get_lua_state();
            return 1;
        });

        lua.register_function("GetCurrentThread", [](const LuaMadeSimple::Lua& lua) -> int {
            lua.set_integer(GetCurrentThreadHash());
            lua.get_lua_state();
            return 1;
        });

        lua.register_function("ExecuteConsoleCommand", [](const LuaMadeSimple::Lua& lua) -> int {
            if (GameEngineThread != GetCurrentThreadHash()) {
                Output::send(STR("Failed to run command. Console commands can ONLY be run in the game thread.\n"));
                lua.set_string("");
                lua.get_lua_state();
                return 1;
            }

            if (IsOblivionInitialized() == 0) {
                Output::send(STR("Failed to run command. Oblivion is not initialized. Load a save or start a new game first.\n"));
                lua.set_string("");
                lua.get_lua_state();
                return 1;
            }

            auto text = lua.get_string().data();
            //Output::send(STR("ExecuteConsoleCommand('{}')\n"), GetWC(text));
            ExecuteCommand(text);
            //Output::send(STR("Console buffer being sent back to Lua: {}\n"), GetWC(OutputBuffer.c_str()));
            lua.set_string(OutputBuffer.c_str());
            lua.get_lua_state();
            return 1;
        });

        lua.register_function("SpawnActor", [](const LuaMadeSimple::Lua& lua) -> int {
            auto text = lua.get_string().data();
            typedef void SpawnActorFuncType(uint64_t, uint64_t);
            auto SpawnActorFunc = (SpawnActorFuncType*)(BaseModuleAdd + 0x65554C0);
            SpawnActorFunc(0, 0x13640);
            lua.get_lua_state();
            return 0;
        });
    }
};

#define CONSOLE_UTILS_API __declspec(dllexport)
extern "C"
{
    CONSOLE_UTILS_API RC::CppUserModBase* start_mod()
    {
        return new ConsoleUtils();
    }

    CONSOLE_UTILS_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        if (ConsolePrint_Detour && ConsolePrint_Detour->isHooked())
            ConsolePrint_Detour->unHook();

        delete mod;
    }
}
