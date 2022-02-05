# Toggle Dark/Light Mode

Single click toggle between dark mode and light mode using an icon in the Windows taskbar notification area (system tray).  This may be useful for anyone working under significantly changing background or reflected light levels.

![Screenshot showing the application icon in the notification area](screenshot.png)

This is an open source ([MIT License](https://github.com/danielgjackson/toggle-dark-light/blob/master/LICENSE.txt)) application, and the project repository is at:

* [github.com/danielgjackson/toggle-dark-light](https://github.com/danielgjackson/toggle-dark-light)


## Installation

1. Download the installer `toggle.msi` from:

    * [Releases](https://github.com/danielgjackson/toggle-dark-light/releases/latest)

2. Double-click to run the downloaded installer.  As Windows will not recognize the program, you may need to select *More info* / *Run anyway*.  Follow the prompts to install the application.

3. The application will run after installation (by default), and an icon for *Toggle Dark/Light Mode* will appear in the taskbar notification area (system tray).  It might be in the *Taskbar overflow* menu behind the **&#x1431;** symbol (this can be changed in the *Taskbar Settings*).  


## Use

1. Left-click the icon to immediately toggle dark/light mode.

2. Use the shortcut key combination <kbd>Win</kbd>+<kbd>Shift</kbd>+<kbd>D</kbd> to toggle between dark and light mode.

3. Right-click the icon for a menu:

    * *Toggle Dark/Light Mode* - Toggles between dark and light modes (same as left-clicking the icon).
    * *Auto-Start* - Toggles whether the executable will be automatically run when you log in.
    * *Save Debug Info* - This will prompt to save a text (`.txt`) file with debugging information.
    * *About* - Information about the program.
    * *Exit* - Stops the program and removes the icon. (If *Auto-Start* is enabled, it will start when you log-in again)


## Advanced Use

The executable can be used to directly set the mode (and optionally exit afterwards):

* `toggle /TOGGLE /EXIT`
* `toggle /LIGHT /EXIT`
* `toggle /DARK /EXIT`

---

  * [danielgjackson.github.io/toggle-dark-light](https://danielgjackson.github.io/toggle-dark-light)
