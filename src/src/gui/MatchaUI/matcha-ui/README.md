# Matcha UI

Free ImGui / DirectX 11 menu look

This is **UI only**. Toggles, sliders, and color pickers do not attach to a game or run features. The Visuals **ESP Preview** window is the only live piece — it shows a built-in 3D mannequin.

There is no loader, no loading screen, and no Spotify widget.

## Build

Open `matcha.sln` in Visual Studio and build **Release | x64**, or:

```
MSBuild.exe matcha.sln /p:Configuration=Release /p:Platform=x64
```

Output: `build\matcha.exe`

## Use

Run `matcha.exe`.

- **Insert** closes the menu (fade + zoom out), then the app exits
- Open it again by running the exe

Drop this folder into your own project if you only want the chrome. Hook your own settings to the existing `settings::` variables if you wire it up later.
