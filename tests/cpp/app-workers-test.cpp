#include "varn/runtime/App.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace
{
// Sets `VARN_WORKERS` for one test and restores it when the test ends.
class WorkersVariable
{
public:
    explicit WorkersVariable(const char* value)
    {
        const char* previous = std::getenv("VARN_WORKERS");
        if (previous != nullptr)
        {
            saved = previous;
            hadValue = true;
        }

        assign(value);
    }

    ~WorkersVariable()
    {
        assign(hadValue ? saved.c_str() : nullptr);
    }

    WorkersVariable(const WorkersVariable&) = delete;
    WorkersVariable& operator=(const WorkersVariable&) = delete;

private:
    static void assign(const char* value)
    {
#if defined(_WIN32)
        _putenv_s("VARN_WORKERS", value != nullptr ? value : "");
#else
        if (value == nullptr)
        {
            unsetenv("VARN_WORKERS");
            return;
        }

        setenv("VARN_WORKERS", value, 1);
#endif
    }

    std::string saved;
    bool hadValue = false;
};
} // namespace

namespace varn::runtime
{

TEST(AppWorkers, ReadsAWholeCountWithinTheBound)
{
    const WorkersVariable variable("4");
    EXPECT_EQ(App::workerCount(), 4);
}

TEST(AppWorkers, RefusesACountItWouldOtherwiseBend)
{
    for (const char* value : {"0", "-2", "1025", "abc", "4x", "2.5", " 3"})
    {
        const WorkersVariable variable(value);
        try
        {
            App::workerCount();
            ADD_FAILURE() << "The value \"" << value << "\" should be refused.";
        }
        catch (const std::invalid_argument& ex)
        {
            EXPECT_NE(std::string(ex.what()).find("The variable \"VARN_WORKERS\" must be a whole number from 1 to 1024"), std::string::npos) << ex.what();
        }
    }
}

} // namespace varn::runtime
