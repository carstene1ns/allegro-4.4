/*         ______   ___    ___
 *        /\  _  \ /\_ \  /\_ \
 *        \ \ \L\ \\//\ \ \//\ \      __     __   _ __   ___
 *         \ \  __ \ \ \ \  \ \ \   /'__`\ /'_ `\/\`'__\/ __`\
 *          \ \ \/\ \ \_\ \_ \_\ \_/\  __//\ \L\ \ \ \//\ \L\ \
 *           \ \_\ \_\/\____\/\____\ \____\ \____ \ \_\\ \____/
 *            \/_/\/_/\/____/\/____/\/____/\/___L\ \/_/ \/___/
 *                                           /\____/
 *                                           \_/__/
 *
 *      Configuration defines for use with 3DS.
 *
 *      See readme.txt for copyright information.
 */

#ifndef AL3DSCFG_H
#define AL3DSCFG_H

#ifndef SCAN_DEPEND
   #include <fcntl.h>
   #include <unistd.h>
#endif

#ifndef ALLEGRO_NO_MAGIC_MAIN
   #define ALLEGRO_MAGIC_MAIN
   #define main _mangled_main
   #undef END_OF_MAIN
   #define END_OF_MAIN() void *_mangled_main_address = (void *) _mangled_main;
#else
   #undef END_OF_MAIN
   #define END_OF_MAIN() void *_mangled_main_address;
#endif

/* TODO: Use the configure script. */
/* A static auto config */
#define ALLEGRO_HAVE_DIRENT_H   1
#define ALLEGRO_HAVE_INTTYPES_H 1
#define ALLEGRO_HAVE_STDINT_H   1
#define ALLEGRO_HAVE_SYS_TIME_H 1
#define ALLEGRO_HAVE_SYS_STAT_H 1

/* Describe this platform */
#define ALLEGRO_PLATFORM_STR "Nintendo 3DS"
#define ALLEGRO_USE_CONSTRUCTOR

#define ALLEGRO_LITTLE_ENDIAN

/* Exclude ASM */
#ifndef ALLEGRO_NO_ASM
   #define ALLEGRO_NO_ASM
#endif

/* Arrange for other headers to be included later on */
#define ALLEGRO_EXTRA_HEADER "allegro/platform/al3ds.h"

#endif
