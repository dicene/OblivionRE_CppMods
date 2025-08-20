# MyMods
## ConsoleUtils v1.1 - https://github.com/dicene/OblivionRE_CppMods/releases/tag/1.1
### Adds Lua Function `ExecuteConsoleCommand(command)`
This new function returns the results of a Console Command as a string. This SHOULD work correctly with OBSE64 since the new functionality in OBSE64 is built into the same list of console commands that the `ExecuteConsoleCommand` method is using, but I haven't personally tested it thoroughly.

> [!CAUTION]
> Call only from in the Game Thread, either via hooking functions (like `ReceiveTick` on an Actor), or via `ExecuteInGameThread` (although it's worth noting that there's a bug in the current release of UE4SS where `ExecuteInGameThread` sometimes does not execute inside of the game thread. The function does detect if you attempt to call it from an incorrect thread and prevents itself from firing to avoid crashing the game.

### Example Usage:
```lua
local function GetQuestStage(questName)
    local result = ExecuteConsoleCommand(string.format("getstage %s", questName))

    if not result:match("GetStage") then
        return -1
    end

    return tonumber(result:match("GetStage >> ([0-9%.]+)"))
end
```

Examples of how you should and shouldn't call it are provided in the `ConsoleUtils\scripts\main.lua` file, including an example of queueing and triggering a console command on a hook on the Player's `ReceiveTick` method, ensuring it's run from within the game thread. Press `CTRL+NumPad1` to run a test that checks a few quest stages to demonstrate use of the returned text from a console command.

Potential future features:
 - Maybe modify it to work in `ExecuteWithDelay` or `ExecuteAsync` calls by blocking the calling lua function until the next engine tick and completing it from within that tick.
 - Add a custom event handler so BP mods can make use of console commands for the countless things that they provide access to that you can't normally do from in a BP mod.

Changelog:
 - 1.1 - https://github.com/dicene/OblivionRE_CppMods/releases/tag/1.1
   - Updated mod to use AoBs that seem to be function in versions 1.0, 1.1, and 1.2 of Oblivion Remastered. Hopefully the AoBs are in stable enough places to avoid getting broken during future updates, but if this mod ever breaks with an Oblivion update, I should be able to find the time to locate new AoBs.
 - 1.0 - https://github.com/dicene/OblivionRE_CppMods/releases/tag/1.0
