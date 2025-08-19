# MyMods
## ConsoleUtil (C++ version)
### Adds Lua Function `ExecuteConsoleCommand(command)`
This new function returns the results of a Console Command as a string. Call only from in the Game Thread, either via hooking functions (like `ReceiveTick` on an Actor), or via `ExecuteInGameThread` (although it's worth noting that there's a bug in the current release of UE4SS where `ExecuteInGameThread` sometimes does not execute inside of the game thread.
Initial release and details at: https://github.com/dicene/OblivionRE_CppMods/releases/tag/ConsoleUtils
