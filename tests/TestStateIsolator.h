#include <gtest/gtest.h>
#include <map>
#include <string>

/** Whenever you need to provide a "fake" implementation 
 *  for a static class or function (e.g. System::GetNow())
 *  your dummy implementation will be "global" because it is 
 *  static. This can lead to problems when multiple tests are
 *  run in parallel and influence each other. Or when one test
 *  modifies the state of the "static" function/class and thus
 *  creates an unknown initial condition for a following test.
 *  This can lead to unreliable, flaky tests.
 * 
 *  This little helper class is designed to deal with that.
 *  It keeps a unique "State" for each test and returns
 *  the correct "State" based on the test that's currently
 *  executing. This way, each test gets its own, unique 
 *  "State" and all tests stay separated from each other, 
 *  even though they call the same static functions.
 */
template <typename State>
class TestStateIsolator
{
  public:
    State& GetStateForCurrentTest()
    {
        return testStates_[GetCurrentTestName()];
    }

    bool HasStateForCurrentTest()
    {
        const auto state = testStates_.find(GetCurrentTestName());
        return state != testStates_.end();
    }

    void cleanupCurrentTestState()
    {
        const auto state = testStates_.find(GetCurrentTestName());
        if(state != testStates_.end())
            testStates_.erase(state);
    }

  private:
    std::string GetCurrentTestName()
    {
        return ::testing::UnitTest::GetInstance()->current_test_info()->name();
    }
    std::map<std::string, State> testStates_;
};