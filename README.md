# Auto Deafen for Eclipse

A Windows Geode mod that integrates with Eclipse's **Player** section.

## Features

- Press **Left Shift** while playing to open the setup menu.
- Enable or disable Auto Deafen.
- Choose the level percentage where your voice app is deafened.
- Choose the percentage where it is undeafened.
- Click **Set Key**, then press any keyboard key to bind it.
- Also includes an **Auto Deafen Setup** button in Eclipse's Player tab.

## Voice-app setup

Create a **Toggle Deafen** keybind in Discord (or another voice app), then choose the same key in this mod. The mod sends that key once at the deafen percentage and again at the undeafen percentage.

## Building

Install Geode SDK 5.3+ and its Windows binaries, then run:

```sh
geode build
```

Place the resulting `.geode` file in `Geometry Dash/geode/mods` alongside Eclipse.
