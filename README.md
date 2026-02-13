# Nioh 2 Native Mod Loader

A **dinput8.dll** proxy for Nioh 2 that automatically scans and injects DLL mods from a `mods/` folder next to the game executable.

## How it works

1. **DLL proxy** — The loader replaces the game’s `dinput8.dll`. All DirectInput API calls are forwarded to the real `dinput8.dll` from your system directory, so the game’s input still works normally.

2. **Mod injection** — On startup, a background thread waits a few seconds (to let the game initialize), then scans the **`mods/`** folder next to the game .exe and calls `LoadLibrary` on every **`.dll`** file found there. Mod DLLs are loaded into the game process in the order the filesystem returns them.

3. **No config** — Drop your mod DLLs into `mods/` and run the game. No config file or launcher required.

## Setup

1. Build the project (Visual Studio, x64) or use a pre-built `dinput8.dll`.
2. Copy **`dinput8.dll`** into your **Nioh 2 game folder** (the folder that contains `Nioh2.exe`).
3. Create a **`mods`** folder in that same game folder (the loader will create it if it doesn’t exist, but it will be empty).
4. Put your native mod **.dll** files inside **`mods/`**.
5. Start Nioh 2 as usual.

Your layout should look like:

```
Nioh2.exe
dinput8.dll      ← this loader
mods/
  SomeMod.dll
  AnotherMod.dll
```

## Building

Open `dinput8/dinput8.slnx` in Visual Studio and build for **x64** (Debug or Release). The output `dinput8.dll` goes in the game folder.

## Disclaimer

This project is **for personal use**. It may not be actively maintained; use at your own risk. Compatibility with future game or Windows updates is not guaranteed.

## License

MIT — see [LICENSE](LICENSE).
