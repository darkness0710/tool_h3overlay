## ABOUT THIS FORK

This is a **modified fork** of `h3overlay`, originally written by **Grek** <gitgrek@gmail.com>.

- Upstream: <https://gitlab.com/Grekern/h3overlay>
- Modified by darkness0710, 2026-09-07
- Licensed under **GNU GPL v3** — see [LICENSE.md](LICENSE.md)

Published history here is squashed into a single release commit.
The full upstream commit history lives in the GitLab repository linked above.

----------

## USAGE
Simply start this application and it should automatically try to attach to Heroes 3 HotA.
It will look for a process named "h3hota.exe" or "h3hota HD.exe".
If the process is named differently, it will not work.

As soon as a game is active, the overlay should be automatically populated.
You can override the values in the "controller".


## HOW IT WORKS
This application will attach to the Heroes 3 HotA game and 
read the game memory directly. This means that this 
application is vulnerable to break with game updates.

When Heroes 3 is updated, the data this application reads can move to a
different place in memory. Reading the old place usually still succeeds, it
just returns something else, so a break can show up as wrong names and flag
colors rather than as an obvious failure. The overlay now checks that what it
reads still looks like real player data, and shows a warning in the status bar
of the controller window when it does not. If you see that warning, this build
needs new offsets, so do not trust what the overlay is showing.


## FEATURES
All data can be automatically fetched if no override is given.
* **Title** - The map name. Will include the map variant for some templates.
* **Players** - It will show the two first human players
  in the current match. One to the left and one to the right.
  The flag color in the overlay will match the in game color.
* **Rating** - The overlay will read the rating from the game, not online.
  The rating in the game client will only update if the user can see
  the rating, which means the user has the "hover" the mouse on
  the other player in the lobby
* **Score** - The overlay will track wins by checking for the "YOU WIN/LOSE"
  popup for a ranked match. This feature is prone to break when the game
  is updated.
* **Start hero/town** - The behavior can be changed in the settings to allow
  the overlay to detect when the player opens the Thieves' Guild and
  use the "best hero" instead of starter hero if available.
* **Trade** - This requires the overlay to be running
  while the trade happens in the lobby chat. Else it will miss that event


## BUILDING
This was compiled using QtCreator, Qt 6, GCC, 64 bit.

* Get Qt [here](https://www.qt.io/download-open-source) 
([Direct](https://www.qt.io/download-qt-installer-oss) link to download page)
The default installation should include MinGW.
* Open QtCreator and load the project file "h3overlay.pro"
* Compile.

To make a standalone package, the binary need some libraries in the same directory as the exe.
This can be done with the `windeployqt.exe` which is in the \<Qt installation>/mingw_64/bin/
```
  $ windeployqt.exe <path to the directory where the compiled exe is>
```
It must have the same build environment as when compiling.

Can be added as a custom build step in QtCreator. In the "Projects" tab in QtCreator, 
add a "Custom Process Step" with the following:
```
Command: %{ActiveProject:QT_HOST_BINS}\windeployqt.exe
Argument: %{buildDir}\release
Working directory: %{buildDir}
```

## CONTACT
email: gitgrek@gmail.com
