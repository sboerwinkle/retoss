# Retoss

This project is an excuse for me to learn about many little things that go into game engines / game design. It is kind of a game, but if things seem a bit weird that's because it's more meant to be "fun to work on" than, like, "profitable" or something.

Basically it's a team deathmatch shooter to be played with friends.

If you think this is kind of neat, check out [Bittoss](https://github.com/sboerwinkle/bittoss), which is kind of this project's predecessor before I decided to rework some foundational design decisions.

## On Windows

On Windows, you'll probably want to take the latest pre-compiled build on Github.
You can technically compile it from source (see `building_for_windows.txt`), but making this easy is not a priority for me.

## On Linux

On Linux, you have to compile from source, because that's less work for me and I assume you're probably comfortable with a shell script.

Use `build.sh` to compile the project. You'll probably need to install some dependencies, but if you're familiar with how development packages are named for your distro it shouldn't be too bad.

## Server

The server is one python script, at `server/server.py`. Defaults to serving on port 15000, or you can pass an argument.
You have to start the server even if you're just playing by yourself!

## Launching the game

The executable (`game`, or `game.exe`) takes two arguments:

- Mandatory: Server IP. The Linux build also accepts hostnames here if you have one.
- Optional: Server Port. Defaults to 15000.

Once it connects, it should pop open a browser where you can do basic player setup (like name and team) and set some settings.

## Playing the game

Info in this section might be out of date!
I kinda forgot this project even had a `README.md` for a while,
and I don't plan to update this file every time I add or change something.

Controls are (at time of writing) WASD, space, left and right mouse, and C (to descend, only while dead).

Currently the only levels are:

- `/lv_tdm1` - the default
- `/lv_peaks` - the other TDM (team deathmatch) level

The game considers it time to start keeping score when everybody has a team chosen (consider `/shuffle`), and then someone dies (for any reason).

Weapons are a work in progress and probably not balanced.
The ability to set your max health is to help balance uneven teams.

There is also one more level (`/lv_swarm`), but it's kind of just leftovers (or if you want to be generous, an "art piece"). There's no scorekeeping (nor rounds) on that level, so you'll have to `/respawn` if you die.

## Contributors

sboerwinkle: Yours truly, primary author

mboerwinkle:

- IPv6 support, server hardening, and host resolution for networking
- Starter graphics code
- Some linear algebra functions
- JSON reading/writing
- Maybe more, IDK
