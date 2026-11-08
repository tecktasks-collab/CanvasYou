# Phase 3 guide

This is the handoff for the person starting Phase 3 while Phase 2 is still being built.

Phase 3 is: get a picture out of this app and into OBS, and also save that same picture as a video file. Phase 2 is still deciding which screen shows which part of the show. Those are separate jobs. Do not rewrite Phase 2 to get OBS working.

---

## What this app is

This is a Windows program for running a live show: slides, video backgrounds, and screens in a room.

Someone edits a deck, saves it, and presents it. The files are this app's own format. It does not open PowerPoint files.

Think of it as three programs that start together:

| Program | What it does |
|---|---|
| `Initializer.exe` | The only one you double-click. It starts the other two, and it shuts the video program down when you close the app. |
| `Window.exe` | The windows you see. Slides, buttons, playlists, and the decision of what should be on screen right now. |
| `Gpu.exe` | A hidden helper. It plays video files and paints them into the windows that `Window.exe` owns. |

Always start `Initializer.exe`. `Window.exe` and `Gpu.exe` refuse to start on their own.

There is also `Basic.exe`. That is a simpler backup used when the computer cannot do normal graphics. You can ignore it for Phase 3.

`dxgi_xproc.cpp` and `dxgi_child_test.cpp` are old experiments. They are not part of the running app.

Saved settings live in a small database at:

`%LOCALAPPDATA%\PresentationSoftwareDev`

If a file named `data_root.txt` sits next to the exe, that folder is used instead.

---

## Words you will see in the code

**Slide.** One page of the deck. Drawn by this app with normal Windows drawing (pictures and text).

**Live.** The show is actually being sent to the screens. Editing a slide does not send it live by itself. Someone has to put the show on the screens first.

**Look.** A saved setup for the room. There are three: Sunday AM, Sunday PM, and Overflow. Each look remembers, for every screen, whether the picture is copied onto every display or stretched across them, and whether the background and the slide are turned on. Phase 2 owns this.

**Applied look.** The look that is actually on the screens. The look tab you happen to be viewing can be a different one. OBS should follow the applied look.

**Canvas.** The full picture, before it is cut up for each display. Usually 1920 by 1080. A screen can be set to another size.

**Mirror.** Every display shows the whole canvas.

**Span.** The canvas is stretched across several displays. Each display shows its own slice.

**Pipe.** A private message channel between two programs on the same computer. `Window.exe` sends play, pause, and seek commands to `Gpu.exe` through `\\.\pipe\PresentationSoftwareGpu`.

**Shared memory.** A block of memory both programs can see. The slide picture is handed to the video helper this way, under the name `Local\PresentationSoftwareSlideOverlay`.

**OBS.** A separate, free program people use to stream and record. We want OBS to receive our show as a clean picture, the same way a camera would.

---

## How a picture gets onto a screen today

The function in charge is `LiveOutput_Push()` in `scratch.cpp`. Buttons and menus only change the current state. That function is what actually updates the screens.

There are two ways a frame gets drawn:

1. **A normal slide, no video.** `Window.exe` draws the slide itself, straight into each window.
2. **A video background, or a video sitting on the slide.** `Window.exe` sends a message to `Gpu.exe`. `Gpu.exe` decodes the video file, draws it into a texture, then copies the right crop of that texture into each window. The slide is copied on top of the video from shared memory.

A texture here just means a picture that lives on the graphics card.

The important gap: those two layers (video and slide) are only combined at the last moment, separately inside each window. There is no single finished picture of the whole show sitting somewhere that another program can read.

That finished picture is what Phase 3 has to create.

---

## What you are building

One full-size picture of the show (the canvas). Build it after the video and the slide have been combined, and before the picture is sliced up for each physical screen.

Then two things read that same picture:

1. **OBS.** A small OBS add-on (a "source") that opens the shared picture and shows it inside OBS. This app still decides what the show looks like. OBS only displays it and, if the user wants, streams it. Start with the picture going out. Buttons back from OBS (start, stop, change look) can wait.
2. **A recording in this app.** Save that same picture as a video file, even when OBS is not open. This app already uses Windows Media Foundation to *play* video (`d3d_video.cpp`). Use that same Windows media stack to *save* video.

`session_recorder.cpp` is a debug tool. If a folder named `screenshots` exists next to the exe, the app saves a photo of the front window about 10 times a second and shows a popup when it starts. That is for debugging crashes. It is not the recording feature. Rename or delete the `screenshots` folder to turn it off.

### Sound

Sound comes from two places, and they are not on the same clock:

- Playlist music is played inside `Window.exe` (`audio.cpp`).
- Sound inside a video file is played inside `Gpu.exe`.

A recording has to include both, or it has to be an explicit choice to keep only one of them. Video position and mute state are already published for other code to read, under the name `Local\PresentationSoftwareGpuStatus`.

---

## What you actually do

You are adding a second copy of the show. The screens in the room keep working the way they do now. Your copy is the whole picture, at canvas size, updated every time the show updates. OBS reads it. The recorder reads it. Neither one draws slides.

Do the work in this order. Each step is done when you can see the result without guessing.

### 1. Make the finished picture exist

Add new files for this. Leave `scratch.cpp` and `d3d_video.cpp` alone except for one call each, described in the next section.

The picture is a block of shared memory, the same idea as the slide overlay that already exists (`Local\PresentationSoftwareSlideOverlay`). Give yours its own name, for example `Local\PresentationSoftwareProgramFrame`. Put a small header in front of the pixels:

- width and height (the canvas, usually 1920 by 1080)
- a counter
- the pixels themselves, blue-green-red-alpha, top row first

Every time the show changes, write it like this:

1. Add 1 to the counter. Readers treat an odd number as "someone is writing, skip this."
2. Copy the pixels in.
3. Add 1 to the counter again. An even number means the picture is safe to read.

A reader looks at the counter, copies the pixels, and looks at the counter again. If the number changed, or it was odd, throw that copy away and try the next frame.

When the show is cleared, write a black picture. An old slide left in the buffer will show up in OBS after the screens have already gone black.

You will publish from two places, because the app draws in two ways:

- **Video is playing.** Inside `Gpu.exe`, after the video frame is drawn, combine it with the slide once at full canvas size, then publish that. The code that already crops the picture into each room display stays as it is.
- **No video.** Inside `Window.exe`, the slide bitmap you already draw is the whole picture. Publish that too.

Skip the second one and a title slide never reaches OBS. The video path never runs for those slides.

### 2. Prove the picture with a tiny viewer

Build this before you touch OBS. A small extra program, its own exe, that:

- opens the shared picture
- shows it in a window, or saves one frame as a PNG when you press a key
- follows the counter rules above so you never save a half-written frame

Run `Initializer.exe`, put a slide on the screens, and look at the viewer.

Check three things:

- A slide with no video matches what is on the screen, including text.
- A video background shows the video with the slide on top, as one picture.
- Clear the screens. The viewer goes black.

When those three work, the app side of the job is done. OBS and the file recorder are just two more readers of this same picture.

### 3. Show that picture in OBS

This part is a separate little project. It is an OBS add-on, not more code inside `Window.exe`. OBS calls it a source. The person using OBS adds it the way they would add a camera.

The add-on, each frame:

- opens the same shared picture
- waits until the counter is even and steady
- hands those pixels to OBS

In OBS, the preview should match the viewer from step 2. Change slides, start a video background, span one picture across two room displays. OBS keeps showing the whole canvas. The room displays keep showing their own slices. You do not teach OBS about looks, spans, or slides.

Sending commands back from OBS (start recording, change the look, and so on) waits until the picture path is solid.

### 4. Save that picture as a video file

Read the same shared picture and write an `.mp4`. Use Windows Media Foundation for the saving side. `d3d_video.cpp` already uses it to play video, so the encode path should use that stack too (`IMFSinkWriter` is the piece that writes the file).

The recording has to work with OBS closed. OBS is one viewer. The file is another.

A first version can be rough on the interface: a button, or even a key, that starts and stops the file. What matters is the file.

A good file:

- plays in a normal player
- matches the viewer from step 2 for the whole clip
- ends cleanly when you stop, so the player shows a duration and the last moment of the show
- goes black in the file when the screens are cleared

### 5. Put sound in the file

Do this after the picture in the file is right. A silent file that matches the screens is a finished step 4.

Then add sound. It comes from two players:

- Playlist music, played inside `Window.exe` (`audio.cpp`)
- The audio track of a video file, played inside `Gpu.exe`

They do not share a clock. For the first recording that has sound, take the video file's audio when a video is playing, and the playlist when it is the thing making noise. Write down which one the file contains. Mixing them into one track is the follow-up, once each one records cleanly on its own.

`Local\PresentationSoftwareGpuStatus` already tells you whether the video helper is playing, where it is in the file, and whether it is muted. Use that instead of guessing.

### What a work session looks like

1. Close `Initializer`, `Window`, and `Gpu` so the build can replace the exe files.
2. Build the three programs in [How to build and run](#how-to-build-and-run). Build your viewer the same way when you change it.
3. Start `Initializer.exe`.
4. Put a still slide on the screens. Look at your viewer.
5. Put on a video background. Look again.
6. Clear the screens. The viewer goes black.
7. Only then open OBS and add the source. Repeat those three checks there.
8. Record about ten seconds covering a still slide and a video. Play the file back.

Stay on the step that fails. An OBS plugin cannot fix a picture that the tiny viewer already shows wrong.

---

## Where to hook in, and what to leave alone

Phase 2 is still changing the look editor. Almost all of that is in `scratch.cpp`: the look checkboxes, `LookOutputRects`, and `LiveOutput_CollectOutputs`. Screen windows themselves are created in `manage_screens.cpp`. Expect those to keep changing. Do not rebuild them.

Add your work in new files, and call into it from the two drawing paths:

- `PresentComposedFrame` in `d3d_video.cpp` — this is the video path. The video picture is `g_frameTex`. The slide is copied on soon after that.
- The normal-slide branch of `LiveOutput_Push` in `scratch.cpp` — this path never talks to `Gpu.exe`.

If you only hook the video path, a title slide with no video will never show up in OBS.

The command message sent to `Gpu.exe` is a fixed-size block (`GpuMessage` in `gpu_protocol.h`). Both programs must agree on its layout. Add a new shared picture on the side. Do not insert new fields into the middle of that message while both phases are being built.

The physical screens should keep using `LookOutputRects` to decide which slice they show. OBS and the recorder always get the whole canvas.

---

## How to build and run

This project is compiled with Visual Studio 2022, 64-bit, using the `cl` compiler. There is no CMake project. `sqlite3.obj` is already in the folder and has to be linked in.

Before you compile, close `Initializer`, `Window`, and `Gpu`. If any of them is still running, Windows will refuse to replace the exe.

Open an "x64 Native Tools Command Prompt for VS 2022", `cd` to this folder, and build the three programs below.

**Window.exe** (the UI):

```bat
cl /nologo /EHsc /Fe:Window.exe ^
  scratch.cpp renderer.cpp manage_screens.cpp edit_view.cpp document.cpp ^
  history.cpp audio.cpp templates.cpp gpu_client.cpp store.cpp boot.cpp ^
  init.cpp window_main.cpp sqlite3.obj ^
  user32.lib gdi32.lib gdiplus.lib comctl32.lib comdlg32.lib shell32.lib ^
  ole32.lib winmm.lib mfplat.lib mf.lib mfuuid.lib ^
  /link /SUBSYSTEM:WINDOWS
```

**Gpu.exe** (the video helper). Leave `init.cpp` out of this one. That file starts the slide drawer, which `Gpu.exe` does not use.

```bat
cl /nologo /EHsc /Fe:Gpu.exe ^
  gpu_main.cpp d3d_video.cpp video_cache.cpp ^
  user32.lib d3d11.lib dxgi.lib ole32.lib oleaut32.lib ^
  gdi32.lib msimg32.lib mfplat.lib mf.lib mfuuid.lib ^
  /link /SUBSYSTEM:WINDOWS
```

**Initializer.exe** (the launcher). Same reason: do not link `init.cpp` here.

```bat
cl /nologo /EHsc /Fe:Initializer.exe ^
  initializer.cpp store.cpp boot.cpp session_recorder.cpp sqlite3.obj ^
  user32.lib gdi32.lib gdiplus.lib d3d11.lib ole32.lib dwmapi.lib ^
  /link /SUBSYSTEM:WINDOWS
```

`templates_main.cpp` is a separate small tool. It is not part of `Window.exe`.

Then run `Initializer.exe`.

---

## How to know it works

- Start `Initializer.exe`. A normal slide with no video shows up in OBS at the full canvas size.
- Put on a video background. OBS shows the video with the slide on top of it, as one picture.
- Set one screen to span across two displays. OBS still shows the whole picture. Each display in the room shows its own half. Cutting the picture up for those displays stays Phase 2's job.
- Record a clip with playlist music and a video that has its own sound. Both are in the file, or the recording makes it obvious which one it kept.
- Clear the screens so they go black. OBS and the recording go black too. They do not keep showing the previous slide.
- Change the slide while video is playing. The new slide appears whole. A torn or half-updated slide means your copy ran at the same time as the writer in `GpuClient_SetSlideOverlay`. That writer bumps a generation counter when a new slide is ready. Read the picture only when that counter is stable.
