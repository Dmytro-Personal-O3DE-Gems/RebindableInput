# Rebindable Input

Runtime key rebinding for [O3DE](https://o3de.org): *players change keys in a settings menu, the game keeps receiving the same input events, and UI can ask which key to show for an action*.

A fork of the engine's **StartingPointInput** with everything a controls menu needs. Bindings are still made in the Editor (a `.keybinds` asset, edited like `.inputbindings`), and the gem sends the very same `StartingPointInput::InputEventNotificationBus` events, so existing listeners keep working without changes.

- **Rebinding API**: a primary and an alternate key per action, separately for keyboard + mouse and for gamepad; conflicts are reported (move the key or allow a duplicate).
- **"Press a key"**: waits for the player's next key and swallows input meanwhile, so menus and the game do not react.
- **Saving**: only the player's changes are stored (`"Interact, keyboard primary = F"`), so new default keys from a game update reach every player who did not change them. Explicit save and revert, for "Apply" and "Cancel".
- **Active device**: knows whether the player uses keyboard + mouse or a gamepad right now; hints follow it.
- **Input hints**: implements [InputHintsContract](https://github.com/Dmytro-Personal-O3DE-Gems/InputHintsContract) — UI asks for an action and gets the key to show ("E", "Space", "LMB", "A").
- **Key names for players**: localization keys + English text (no dependency on any localization gem), letters as printed on the player's keyboard (AZERTY, QWERTZ).

## Contents

- [What is inside](#what-is-inside)
- [Requirements](#requirements)
- [Installation](#installation)
- [Setting up bindings](#setting-up-bindings)
- [Concepts](#concepts)
- [Using it from C++](#using-it-from-c)
- [Where the player's changes are stored](#where-the-players-changes-are-stored)
- [Key names and localization](#key-names-and-localization)
- [Console commands](#console-commands)
- [Known limitations](#known-limitations)
- [Tests](#tests)
- [Status and roadmap](#status-and-roadmap)
- [License](#license)

## What is inside

```
RebindableInput/
├── Code/Include/RebindableInput/
│   ├── RebindableInputBus.h                 # the rebinding API: RebindableInputRequests (+ Interface and bus)
│   └── RebindableInputNotificationBus.h     # saved, key captured / capture cancelled, active device changed
├── Code/Source/
│   ├── RebindableInputComponent.*           # "Rebindable Input": the component on an entity, holds the .keybinds asset
│   ├── Clients/RebindableInputSystemComponent.*  # owns all active bindings, the player's changes, the API, the hints
│   ├── InputEventBindings.h, InputEventGroup.h, InputEventMap.*   # forked from StartingPointInput
│   ├── BindingsRegistry.h                   # internal: component -> system component
│   ├── InputCapture.*                       # "press a key"
│   ├── ActiveDeviceTracker.*                # keyboard + mouse or gamepad
│   ├── InputChannelInfo.*, InputChannelText.*    # engine channel names -> device, kind, text for players
│   ├── PlayerBindingsFile.*, PlayerBindingsStorage.*   # the player's file: JSON, @user@ or SaveData
│   ├── LocalizationKeyMarker.h              # LOCALIZATION_KEY without depending on a localization gem
│   └── RebindableInputConsoleCommands.*     # debug console commands
├── Code/Tests/                              # GoogleTest
├── Registry/assetprocessor_settings.setreg  # the Asset Processor rule for .keybinds
└── gem.json
```

## Requirements

| | |
|---|---|
| Engine | O3DE **26.05** (developed against tag `2605.0`) |
| Gems (enabled automatically as dependencies) | `StartingPointInput` (its event bus is used), `InputHintsContract` (implemented by this gem), `SaveData` (the player's file in release builds) |
| Platforms | developed and tested on Windows; builds for Linux (see [Known limitations](#known-limitations)) |

## Installation

1. Clone the repository somewhere outside the engine, for example `D:\O3DE\UserGems\RebindableInput`.
2. Register it with the engine:
   ```
   o3de register --gem-path D:\O3DE\UserGems\RebindableInput
   ```
3. Enable it in your project: Project Manager → *Configure Gems* → **Rebindable Input**, or add `"RebindableInput"` to `gem_names` in `project.json`. Build the project.

To call the API from your own gem or project code, add `"RebindableInput"` to the `dependencies` of your `gem.json` and `Gem::RebindableInput.API` to the `BUILD_DEPENDENCIES` in CMake.

## Setting up bindings

1. **Create a `.keybinds` file.** It is the same data as an `.inputbindings` file of StartingPointInput, under this gem's own type. The easiest way is to convert an existing `.inputbindings`: copy it, rename it to `.keybinds`, and replace these type ids in the text:

   | Class | StartingPointInput | RebindableInput |
   |---|---|---|
   | `InputEventBindingsAsset` | `{25971C7A-26E2-4D08-A146-2EFCC1C36B0C}` | `{469F3E72-B34D-47C9-BF4E-6953C2E2B237}` |
   | `InputEventBindings` | `{14FFD4A8-AE46-4E23-B45B-6A7C4F787A91}` | `{0FF51476-1021-4F6F-9601-CAA4090ABB96}` |
   | `InputEventGroup` | `{25143B7E-2FEC-4CC5-92FE-270B67E79734}` | `{2119230A-FFFD-456B-AF0B-B71346C7B33B}` |
   | `InputEventMap` | `{A14EA0A3-F053-469D-840E-A70002F51384}` | `{7C4D4A6D-2833-4BEC-8BD3-D003349663A8}` |
   | `ThumbstickInputEventMap` | `{4881FA7C-0667-476C-8C77-4DBB6C69F646}` | `{30B0CA40-3ABD-4630-BE22-C5B48205C5CA}` |
   | `vector<InputEventGroup>` | `{E7A38039-E414-5322-B384-210475DA7732}` | `{5EBAF9EC-A548-54DE-A02A-F362215629E7}` |
   | `vector<InputSubComponent*>` | `{7B0B6F41-794A-5CFF-8275-91A3137E747D}` | `{D58CA730-AF3E-571B-8F7C-0FEEE5628162}` |

   The container ids are not written in the code: the engine derives them from the element types, so they change together with the class ids.

2. **Add the component.** On the entity that had StartingPointInput's **Input** component (usually the player), add **Rebindable Input** (*Gameplay*) instead and pick the `.keybinds` file in *Input to event bindings*. Remove the old **Input** component: with both, every event is sent twice. *Local player index* works as in the Input component.

3. **Keys per action.** In the `.keybinds` file, the first binding of a device group is the **primary** key and the second the **alternate** one. A third binding of the same device group is disabled with a warning: a settings menu could not show or change it.

Saving the `.keybinds` file while the game runs reloads it (hot reload); the player's changes are applied on top again.

## Concepts

| Term | Meaning |
|---|---|
| **Action** | The event name of an input group in `.keybinds`, e.g. `"Interact"` — the same name your code listens to through `InputEventNotificationBus`. |
| **Device group** | `DeviceGroup::KeyboardMouse` or `DeviceGroup::Gamepad`. Every action has its own slots in each group. |
| **Slot** | `BindingSlot::Primary` or `BindingSlot::Alternate`. A slot holds one channel, or is empty. |
| **Channel** | The engine's name of a key, button or axis: `keyboard_key_alphanumeric_E`, `mouse_button_left`, `gamepad_button_a`. Gamepad buttons are named by position: `gamepad_button_a` is the bottom face button (A on Xbox, Cross on PlayStation). |
| **Kind** | A slot only takes a channel of the kind the action already uses: a button for a button, mouse movement for mouse movement, a stick for a stick. Moving the camera with arrow keys is not possible by design. Triggers and stick directions count as buttons. |
| **Player's changes** | Slots that differ from `.keybinds`. They apply to every entity with the action and are what gets saved. |

## Using it from C++

```cpp
#include <RebindableInput/RebindableInputBus.h>
#include <RebindableInput/RebindableInputNotificationBus.h>

using namespace RebindableInput;
RebindableInputRequests* rebinding = RebindableInputInterface::Get(); // nullptr if the gem is not active
```

All calls are on the main thread. Every change of a binding takes effect at once and sends `InputHintsContractNotifications::OnInputHintsChanged`.

### Building a controls menu

```cpp
for (const AZStd::string& action : rebinding->GetActions())   // in the order of the .keybinds files
{
    // Empty optional: an empty slot.
    AZStd::optional<SlotBinding> key = rebinding->GetBinding(action, DeviceGroup::KeyboardMouse, BindingSlot::Primary);
    // key->m_channelName: "keyboard_key_edit_space"; key->m_key + key->m_defaultText: the text to show,
    // see Key names and localization
}
```

`GetActions()` lists the actions of the entities that are active right now: show the menu while the player entity exists.

### "Press a key"

```cpp
// 1. Start waiting for a key. Nothing is bound yet.
rebinding->StartCapture("Interact", DeviceGroup::KeyboardMouse, BindingSlot::Primary);

// 2. The key arrives as a notification (RebindableInputNotificationBus::Handler):
void MyMenu::OnChannelCaptured(const AZStd::string& action, DeviceGroup group, BindingSlot slot, const AZStd::string& channel)
{
    // 3. Ask about a conflict, e.g. with a pop-up "This key is used by Jump"...
    if (AZStd::optional<BindingConflict> conflict = rebinding->FindConflict(action, group, slot, channel))
    {
        // conflict->m_actionName, conflict->m_slot
    }
    // 4. ...and bind: Move takes the key away from the other action, AllowDuplicate keeps it there too.
    SetBindingResult result = rebinding->SetBinding(action, group, slot, channel, ConflictMode::Move);
}

void MyMenu::OnCaptureCancelled(const AZStd::string& action, DeviceGroup group, BindingSlot slot) { /* Escape, Start, CancelCapture */ }
```

How the capture behaves:
- It starts with the next frame, so the key that pressed the menu button (Enter, gamepad A) is not taken.
- Only a channel of the slot's device group and kind is taken; analog channels (triggers, sticks) must pass half way. Until then **all input is swallowed**: above UI (LyShine) and the game, below ImGui.
- The taken key stays swallowed until it is released, so the new binding does not fire at once.
- **Escape** and gamepad **Start** cancel. They are reserved: `SetBinding` refuses them (`SetBindingResult::ReservedChannel`).

### Other changes

```cpp
rebinding->SetBinding(action, group, slot, "keyboard_key_alphanumeric_F", ConflictMode::Move); // without capturing
rebinding->ClearBinding(action, group, slot);   // any slot may be emptied
rebinding->ResetAction(action);                 // back to .keybinds
rebinding->ResetAll();
```

`SetBinding` returns `Ok`, `UnknownAction`, `UnknownChannel`, `WrongDeviceGroup` (a gamepad button for a keyboard slot), `WrongChannelKind` or `ReservedChannel`.

### Apply and Cancel

```cpp
rebinding->SaveBindings();    // asynchronous: the result comes as OnBindingsSaved(bool success)
rebinding->RevertBindings();  // back to the last successful save or load
```

The player's changes are loaded automatically when the gem starts. Nothing is saved without `SaveBindings`.

### Active device group

```cpp
DeviceGroup group = rebinding->GetActiveDeviceGroup();
// RebindableInputNotifications::OnActiveDeviceGroupChanged(DeviceGroup group)
```

- Input switches the group: keys, mouse buttons and the wheel; gamepad buttons, and triggers and sticks past half way. Mouse movement switches only once it adds up to a real movement within a short time (150 sensor counts in 0.2 s), so a bumped desk or a drifting stick does not switch.
- Before the player's first input, a gamepad that connects switches to the gamepad: a player who starts with a gamepad sees gamepad hints on the first screen.
- Input hints show the key of the active group. If the action has no key in that group, there is no hint (the UI shows its own "unbound" text).

### Input hints

This gem implements [InputHintsContract](https://github.com/Dmytro-Personal-O3DE-Gems/InputHintsContract): `GetInputHint("Interact")` returns the first filled slot of the active device group. UI that depends only on the contract (for example [InteractionSystemUI](https://github.com/Dmytro-Personal-O3DE-Gems/InteractionSystemUI)) works with this gem without knowing it.

## Where the player's changes are stored

The file is JSON (readable, can be edited by hand). It holds the player's changes and the defaults the player has seen, so that a default changed by a game update can be told apart from a duplicate the player made on purpose.

| Setting `/RebindableInput/Storage` | Where | Default in |
|---|---|---|
| `"ProjectUser"` | `<project>/user/RebindableInput/RebindableInputBindings.json` — shared by the Editor and the game launcher | debug and profile builds |
| `"SaveData"` | the SaveData gem: the OS folder for user data (Windows: `%LOCALAPPDATA%\<executable name>\SaveData\`) | release builds |

To choose, add a `.setreg` file to your project's `Registry` folder:

```json
{ "RebindableInput": { "Storage": "SaveData" } }
```

SaveData runs only in game launchers, not in the Editor; there `ProjectUser` is used instead, with a warning.

When the file is loaded:
- a change to a channel that does not exist or belongs to the other device group is dropped, with a warning;
- a change of an action that is not active right now is kept: it may be on another level;
- a change that does not fit the action any more (an update made it another kind) is ignored, with a warning;
- a **new default** (from a game update) that uses a key the player took for another action is emptied, with a warning: **the player's choice wins**.

## Key names and localization

`SlotBinding` and `InputHint` carry a **localization key and an English text**, not a translated text, so this gem does not depend on any localization gem:

| `m_key` | `m_defaultText` | Show |
|---|---|---|
| empty | `E`, `7`, `F1`, `,`, `A` (gamepad) | as it is: the same in every language |
| `input.key.space` | `Space` | the translation of the key; without one, the English text |

The keys are marked with `LOCALIZATION_KEY(...)` in `Code/Source/InputChannelText.cpp` (the marker is repeated in `LocalizationKeyMarker.h`, not included from a localization gem). With [LocalizationGem](https://github.com/Dmytro-Personal-O3DE-Gems/LocalizationGem), its checking tool finds these keys and reports the ones missing in your `en.csv`; until they are added, players see the English texts.

**Keyboard layouts.** The engine names keys by their position on a US QWERTY keyboard. For letters and punctuation the text is taken from the player's layout when it is a single Latin/ASCII character: on AZERTY the key printed **A** shows "A" (the engine calls it `..._Q`). Other alphabets fall back to the QWERTY text (Russian "Ж" → ";"): Latin is printed on those keys too. Digits always show the digit, word keys follow the game's language. The layout is asked every time, so switching it during the game is picked up.

## Console commands

Only in debug and profile builds. In the game, open the console with **Home** (ImGui).

| Command | |
|---|---|
| `rebind_list [action]` | the active device group and the slots of every action |
| `rebind_set <action> <km\|gp> <p\|a> <channel> [Move\|Duplicate]` | bind a channel |
| `rebind_clear <action> <km\|gp> <p\|a>` | empty a slot |
| `rebind_reset [action]` | back to `.keybinds` |
| `rebind_capture <action> <km\|gp> <p\|a> [delay]` | "press a key", then bind with Move. Home, which closes ImGui, is skipped |
| `rebind_save`, `rebind_revert` | save; back to the last save or load |

`km` / `KeyboardMouse`, `gp` / `Gamepad`, `p` / `Primary`, `a` / `Alternate`; not case-sensitive.

## Known limitations

- **Editor**: Escape leaves Game Mode before the game sees it, so cancelling a capture with Escape can only be tested in a game launcher.
- **Editor**: a gamepad turned on after the Editor started is not seen by the engine. Turn it on first.
- **Windows gamepads are XInput only** (engine): Xbox controllers work; PlayStation controllers need Steam Input or similar, and then look like Xbox ones. The controller family (for PlayStation names or icons) is not detected yet.
- **Linux**: no keyboard layout texts from the engine (QWERTY texts are used); not tested yet.
- Mouse movement threshold for capturing a mouse-movement slot is not tuned yet.
- Several local players (split-screen): the local user id is passed through, but the player's changes are global.
- C++ only: no Script Canvas / Lua access yet.

## Tests

`RebindableInput.Tests` (GoogleTest), 29 tests:

- `InputChannelInfoTest`: device group and kind of every engine channel;
- `InputChannelTextTest`: texts and localization keys, a text for every engine channel, which keys follow the keyboard layout and which layout texts are accepted;
- `PlayerBindingsFileTest`: the player's file: round trip, names, invalid and duplicate records.

```
ctest -C profile -R RebindableInput -V        # from the project's build folder
```

## Status and roadmap

Tested in the Editor (O3DE 26.05, Windows) with keyboard, mouse and an Xbox controller: bindings from `.keybinds` with hot reload, the rebinding API, conflicts, "press a key", saving and loading through `@user@`, revert, the active device group, input hints in [InteractionSystemUI](https://github.com/Dmytro-Personal-O3DE-Gems/InteractionSystemUI), translated key names with [LocalizationGem](https://github.com/Dmytro-Personal-O3DE-Gems/LocalizationGem), English and Russian keyboard layouts.

Not verified yet:
- saving through SaveData in a game launcher;
- starting with a gamepad in a game launcher;
- Linux.

Planned:
- **Controller family and icons**: tell PlayStation from Xbox controllers (platform code) and show button icons in hints.
- Script Canvas / Lua access.

## License

[MIT](LICENSE) © 2026 Dmytro Shyshkin
