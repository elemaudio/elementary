
#include "CLIEngine.h"

int main(int argc, char **argv)
{
    return ElementaryCLIMain(argc, argv, [](auto &){});
}