# Language Indicator

Language Indicator is a lightweight Windows application designed for users, especially those with ultra-wide monitors, who find it inconvenient to monitor the current keyboard layout only on the taskbar.

## Features

- **Automatic Language Detection**: Dynamically tracks changes in keyboard layout and displays the current language.
- **On-Screen Language Indicator**: A non-intrusive, semi-transparent popup that appears at the center of the screen, ensuring visibility regardless of monitor size or taskbar position.
- **Tray Icon Integration**: Adds a system tray icon for quick access to application options like exiting.
- **Smooth Fade Animation**: The indicator smoothly fades in and out for a polished user experience.
- **System-Level Integration**: Hooks directly into the Windows API to detect input language changes.

## Current Status

⚠️ **Note**: The application is currently under development, and some functionality is not fully implemented:

- Automatic detection of keyboard layout changes may not work reliably in all cases.
- The language indicator display may have issues updating or fading correctly.
- Proper error handling and stability enhancements are required.

## How It Works

- Monitors keyboard layout changes using a global hook (`WH_KEYBOARD_LL`).
- Displays the current language in a central popup when the layout changes, making it ideal for ultra-wide monitors where taskbar indicators might be less noticeable.
- Uses a timer to hide the indicator after a short delay, ensuring a clean interface.
- Includes a tray icon for user interaction and application control.

## Technology Stack

- **Language**: C++
- **Framework**: Windows API

### Key Components:

- Global keyboard hook for layout detection.
- Layered and transparent window for the visual indicator.
- System tray integration using `Shell_NotifyIcon`.

## Usage

1. Launch the application.
2. The indicator will monitor keyboard layout changes automatically (in progress).
3. Access settings or exit the app via the system tray icon.

## Future Enhancements

- Fixing existing bugs and ensuring stable functionality.
- Configurable indicator position and size.
- Support for additional customization (colors, fonts, transparency).
- Multi-language support for the interface.
- Optimizations for low-resource environments.

## License

Distributed under the MIT License.

This description highlights the current limitations and makes it clear the app is a work in progress.
