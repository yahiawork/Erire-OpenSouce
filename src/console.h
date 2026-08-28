#ifndef ERIRE_CONSOLE_H
#define ERIRE_CONSOLE_H

#include <stdio.h>
#include <stdbool.h>

#include "error.h"

bool er_console_launch_profile(const char *profile, ErError *error);
void er_console_print_profiles(FILE *out);

#endif
