#ifndef ERIRE_STUDIO_BRIDGE_H
#define ERIRE_STUDIO_BRIDGE_H

struct StudioApp;

void studio_bridge_dispatch(struct StudioApp *app, const char *json);

#endif
