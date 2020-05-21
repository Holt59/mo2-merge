# [MO2](https://github.com/ModOrganizer2/modorganizer) plugin for FOMOD installers using C# scripts

This is simple installer plugin for Mod Organizer 2 to handle FOMOD installers containing C# script.

Without this plugin, Mod Organizer 2 will use NCC to install such plugins, but NCC as a few limitations when used with MO2 :

- Mod detection will not work, so the installer will not be able to check for active mods or even existing files.
- Modifying settings file (FalloutNV.ini, etc.) does not work properly with the NCC installer.

This installer is better integrated with Mod Organizer 2, but still has some limitations:

- Files created by the installer (not files extracted) cannot be handled by the installer, thus those will be put in your Overwrites directory and you will be notified of their creation. You will then need to move these files to the installed mods manually.
- Settings files are not updated automatically to avoid overriding settings you do not want. Instead, a dialog will tell you which settings the mods want you to modify at the end of the installation.

If a mod does not create new files or does not modify settings file, you will not see any dialog at the end of the installation.

**Warning:** This plugin currently only works with Mod Organizer 2 development build 2.3.0 alpha 8!
