# Installation

Linux only. The game runs through Steam Play (Proton).

## 1. Install the game

Install **ROB RUN** from Steam.

## 2. Enable Proton

In Steam: **Library** -> right-click **ROB RUN** -> **Properties** -> **Compatibility**.

- Check **Force the use of a specific Steam Play compatibility tool**.
- Select the latest Proton version. Tested with **Proton 10.0-4**.

## 3. Run the game once

Launch the game and wait until it reaches the main menu (or finishes its first
launch setup), then close it completely.

This makes the game create/verify the files we need.

## 4. Find the plugins folder

Default Steam library:

```
~/.local/share/Steam/steamapps/common/Rob Run/ROB RUN_Data/Plugins/x86_64/
```

If the game is installed in another Steam library, use that library's
`steamapps/common/Rob Run/ROB RUN_Data/Plugins/x86_64/` instead. You can also
right-click the game in Steam -> **Manage** -> **Browse local files**.

The folder contains Epic's original `EOSSDK-Win64-Shipping.dll`.

## 5. Back up the original

```sh
cd "$HOME/.local/share/Steam/steamapps/common/Rob Run/ROB RUN_Data/Plugins/x86_64"

mv EOSSDK-Win64-Shipping.dll EOSSDK-Win64-Shipping.dll.original
```

Keep the backup. Steam may restore the original file after a game update.

## 6. Build and copy the DLL

From the repository root:

```sh
./build.sh
```

Then copy the built DLL into the folder from step 4:

```sh
cp shim/bin/EOSSDK-Win64-Shipping.dll \
   "$HOME/.local/share/Steam/steamapps/common/Rob Run/ROB RUN_Data/Plugins/x86_64/"
```

The folder must now contain `EOSSDK-Win64-Shipping.dll` (ours) and
`EOSSDK-Win64-Shipping.dll.original` (the backup).

## 7. Launch the game

Start the game from Steam. If the game is updated, repeat step 6 - Steam will
have replaced the DLL with the original one.

## Uninstall

```sh
cd "$HOME/.local/share/Steam/steamapps/common/Rob Run/ROB RUN_Data/Plugins/x86_64"

rm EOSSDK-Win64-Shipping.dll
mv EOSSDK-Win64-Shipping.dll.original EOSSDK-Win64-Shipping.dll
```

Alternatively, delete the folder's DLL and use Steam's **Verify integrity of
game files**.
