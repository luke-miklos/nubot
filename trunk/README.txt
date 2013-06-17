UAlbertaBot - 2012 AIIDE StarCraft AI Competition

Author:      David Churchill (dave.churchill@gmail.com)
             University of Alberta
Type:        .dll
Race:        Protoss
File IO:     Yes (read / write folder)

Setup:

   Create Windows Environment Variables As Follows:

   BWAPI_DIR - Location of BWAPI libraries
   BOOST_DIR - Location of BOOST 1.47 libraries

Visual Studio Compilation:

   1) Open Visual_Studio_Projects/UAlbertaBot/vs2011/UAlbertaBot.sln
      Open using Visual Studio 11 Beta
	  Ensure 'Platform Toolset' for each project is 'v90'
   2) Select 'Release' build mode
   3) Compile Project 'AdversarialSearch'
   4) Compile Project 'StarCraftBuildOrderSearch'
   5) Compile Project 'UAlbertaBot'
   
Configuration / Notes:

   - UAlbertaBot requires that 'file_settings.txt' be placed local to the bot when running
     This file specifies the tournament read/write folder settings in two lines
	 Using this you can change the location of the read/write folder without recompile

Adversarial Search
	linker settings
$(SDL_DIR)/lib/x86/SDL.lib
$(SDL_DIR)/lib/x86/SDLmain.lib
C:\Users\Dave\Libraries\opengl\opengl32.lib
C:\Users\Dave\Libraries\SDL_image-1.2.12\VisualC\Release\SDL_image.lib
C:\Users\Dave\Libraries\FTGL\win32_vcpp\Build\ftgl_static_MT.lib
C:\Users\Dave\Libraries\FTGL\win32_vcpp\Build\ftgl_dynamic_MT.lib
C:\Users\Dave\Libraries\SDL_gfx-2.0.23\Release\SDL_gfx.lib





