#pragma once

#include <functional>

#include <elem/Runtime.h>

/*
 * Your main can call this function to execute the realtime command line loop. Before audio playback
 * starts, your initCallback will be called with a reference to the elem::Runtime<float> instance
 * for your own initialization needs (i.e. registering new node types or adding shared resources).
 *
 * The Realtime loop runs only on float runtimes for now, so to avoid a template explosion
 * use a specialized runtime float in this callback.
 */
extern int RealtimeMain(int argc, char **argv, std::function<void(elem::Runtime<float> &)> initCallback);
