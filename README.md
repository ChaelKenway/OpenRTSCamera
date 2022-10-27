# OpenRTSCamera

- Forked from [HeyZoos](https://github.com/HeyZoos/OpenRTSCamera)
- Modified by [Yuzhu](https://github.com/ChaelKenway/OpenRTSCamera) for studying.

- [Installing from GitHub](https://github.com/HeyZoos/OpenRTSCamera/wiki/Installing-from-GitHub)
- [Getting Started](https://github.com/HeyZoos/OpenRTSCamera/wiki/Getting-Started)

## Features

- Smoothed Movement
- Ground Height Adaptation
- Edge Scrolling
- [Follow Target](https://github.com/HeyZoos/OpenRTSCamera/wiki/Follow-Camera)
- [Mouse + Keyboard Controls](https://github.com/HeyZoos/OpenRTSCamera/wiki/Movement-Controls)
- [Gamepad Controls](https://github.com/HeyZoos/OpenRTSCamera/wiki/Movement-Controls)
- [Camera Bounds](https://github.com/HeyZoos/OpenRTSCamera/wiki/Camera-Bounds)

# Changelog

### 0.17.0

- Fix [#27](https://github.com/HeyZoos/OpenRTSCamera/issues/27) by tying camera movement to delta time (thanks [@theMyll](https://github.com/theMyll))
- **This will result in slower movement across the board, if you notice your camera moving more slowly, up the speed values by about 100x. For example, the new camera blueprint speed defaults are 5000**
