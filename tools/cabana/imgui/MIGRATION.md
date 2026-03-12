# Cabana ImGui Migration Plan

## Goal

Replace the Qt Cabana application with a non-Qt app while keeping Cabana functional during the migration.

## Strategy

Build a parallel `cabana_imgui` application and migrate product surfaces onto it incrementally.

Keep these layers shared between the old and new apps:

- stream loading and replay plumbing
- DBC management and commands
- settings persistence and session restore data
- data adapters for messages, detail views, and charts

Keep these layers app-specific:

- windowing and input
- layout and docking/tabbing behavior
- menus, dialogs, and panel rendering

## Migration Order

1. App shell
   - standalone window lifecycle
   - main frame and panel layout
   - keyboard shortcut routing
   - session state save/restore hooks
2. Tool dialogs
   - settings
   - stream selector
   - route info
   - find signal
   - find similar bits
3. Video panel
   - camera playback
   - scrub thumbnails
   - playback controls
4. Messages panel
   - filters
   - sorting
   - selection
5. Message detail panel
   - binary view
   - signal list
   - history/log view
6. Charts
   - multi-chart layout
   - scrub/zoom interactions
   - drag/drop management
7. Qt removal
   - delete Qt-only shell code
   - remove Qt dependency from Cabana build

## Rules

- Do not duplicate business logic between the Qt and ImGui apps.
- Extract view-model/state code before rewriting a complex surface.
- Treat charts as the final large migration item, not the first.
- Keep the Qt app buildable until the ImGui app reaches feature parity.

## Immediate Next Steps

1. Land a minimal non-Qt `cabana_imgui` target.
2. Extract shared startup/session state interfaces from the Qt shell.
3. Port the small modal tools first to validate the new app structure.
4. Move the current video work onto the new app after the shell exists.
