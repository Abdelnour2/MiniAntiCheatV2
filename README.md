# MiniAntiCheatV2
An improvement of the previous project "Mini Anti Cheat" that represents a mini simple Anti-Cheat. Features included are:
## Found in V1:
There is a user mode app (representing a game), and a Driver (the Anti-Cheat). The Anti-Cheat scans for a blacklisted app (in this case Notepad.exe):

If the game is opened, you can't open Notepad
If the game is closed, you can
If Notepad is opened, you can't open the game

## Found in V2:
In this version, a "shield" feature has been added, the shield basically prevent other process from having access to the game, this way another hack will be prevented, which is making a custom program that opens a handle to the game to write information in its memory


# Files:
## MiniAntiCheat.cpp
This is the Driver

## Game.cpp
This is the Game

## MiniAntiCheatCommon.h
A header file that has data used by both the driver and the game
