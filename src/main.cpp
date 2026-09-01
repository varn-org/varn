#include "varn/runtime/App.h"

#include "crash_handler.h"

int main(int argc, char** argv)
{
    varn::diagnostics::CrashHandler::install();

    varn::runtime::App app;
    return app.run(argc, argv);
}
