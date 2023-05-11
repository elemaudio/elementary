
#ifndef ELEMENTARY_CLIENGINE_H
#define ELEMENTARY_CLIENGINE_H

#include <functional>
#include "Runtime.h"

/*
 * Your main can call this function to execute the CLI. Before audio playback
 * starts, your registrationCallback will be called with a reference to the runtime
 * the CLI is using
 *
 * The CLI Engine runs only on float runtimes for now, so to avoid a template explosion
 * use a specialized runtime float in this callback.
 */
extern int ElementaryCLIMain(int argc, char **argv,
                              std::function<void(elem::Runtime<float> &)> registrationCallback);


#endif // SST_ELEMENTARY_BINDINGS_CLIENGINE_H
