#include <eforce/Exception.h>
#include <eforce/ExceptionForcer.h>

#include <catch.hpp>

#include <algorithm>
#include <array>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct BigStruct
{
    std::array<int, 100> arr{};
};

// Note we cannot make these helper functions static as the compiler would optimize away the non-zero case
void ThrowIfZeroStackParameter(BigStruct s)
{
    if (!s.arr.back())
        THROW_REGISETERED_EXCEPTION(std::exception);
}

void ThrowIfNonZero(int x)
{
    if (x)
        THROW_REGISETERED_EXCEPTION(std::runtime_error, "");
}

void CondiditonalThrowAndCatch()
{
    try
    {
        ThrowIfNonZero(0);
    }
    catch (std::runtime_error const&)
    {}
}

void ThrowWithNonConstexprInputIfNonZero(int x)
{
    if (x)
    {
        std::ostringstream oss;
        oss << "Test";
        THROW_REGISETERED_EXCEPTION(std::runtime_error, oss.str());
    }
}

void ThrowWithInputValueIfNonZero(int x)
{
    if (x)
    {
        THROW_REGISETERED_EXCEPTION(int, x);
    }
}

class MyException
    : public std::exception
{
public:
    MyException(std::string input)
        : m_what(std::move(input))
    {}

    char const* what() const noexcept override { return m_what.c_str(); }

private:
    std::string m_what;
};

void ThrowMyExceptionIfNonZero(int x)
{
    if (x)
        THROW_REGISETERED_EXCEPTION(MyException, "My exception");
}

class ExceptionForcerFixture
{
protected:
    eforce::ExceptionInfo const& GetExceptionInfoByFnName(std::string const& name)
    {
        auto exceptionToForce = std::find_if(exceptions.cbegin(), exceptions.cend(), [&] (eforce::ExceptionInfo const& info) {
            return info.parentFn.name == name;
        });

        if (exceptionToForce == exceptions.cend())
            throw std::runtime_error("No matching fn name");

        return *exceptionToForce;
    }

    eforce::ExceptionForcer exceptionForcer;
    std::vector<eforce::ExceptionInfo> exceptions{exceptionForcer.GetExceptions()};
public:
    ExceptionForcerFixture () {}
};

TEST_CASE_METHOD(ExceptionForcerFixture, "Exceptions can be forced and unforced") 
{
    auto exceptionToForce = GetExceptionInfoByFnName("ThrowIfNonZero(int)");
    REQUIRE_NOTHROW(ThrowIfNonZero(0));

    exceptionForcer.ForceException(exceptionToForce.addr);
    REQUIRE_THROWS(ThrowIfNonZero(0));

    exceptionForcer.UnforceException(exceptionToForce.addr);
    REQUIRE_NOTHROW(ThrowIfNonZero(0));
}

TEST_CASE_METHOD(ExceptionForcerFixture, "Nested function calls don't break exception handling") 
{
    auto exceptionToForce = GetExceptionInfoByFnName("ThrowIfNonZero(int)");

    exceptionForcer.ForceException(exceptionToForce.addr);
    REQUIRE_NOTHROW(CondiditonalThrowAndCatch());
}

TEST_CASE_METHOD(ExceptionForcerFixture, "Non constexpr exception input fails to force")
{
    auto exceptionToForce = GetExceptionInfoByFnName("ThrowWithNonConstexprInputIfNonZero(int)");
    REQUIRE_THROWS(exceptionForcer.ForceException(exceptionToForce.addr));

    exceptionToForce = GetExceptionInfoByFnName("ThrowWithInputValueIfNonZero(int)");
    REQUIRE_THROWS(exceptionForcer.ForceException(exceptionToForce.addr));
}

TEST_CASE_METHOD(ExceptionForcerFixture, "Passing parameters on the stack doesn't result in stack corruption")
{
    BigStruct s;
    for (auto& elem : s.arr)
    {
        elem = 0xdeadbeef;
    }

    auto exceptionToForce = GetExceptionInfoByFnName("ThrowIfZeroStackParameter(BigStruct)");
    exceptionForcer.ForceException(exceptionToForce.addr);
    REQUIRE_THROWS(ThrowIfZeroStackParameter(s));

    for (auto& elem : s.arr)
    {
        REQUIRE(elem == 0xdeadbeef);
    }

    exceptionForcer.UnforceException(exceptionToForce.addr);
}

TEST_CASE_METHOD(ExceptionForcerFixture, "Custom exceptions can be forced")
{
    auto exceptionToForce = GetExceptionInfoByFnName("ThrowMyExceptionIfNonZero(int)");

    REQUIRE_THROWS_AS(ThrowMyExceptionIfNonZero(1), MyException);
    REQUIRE_NOTHROW(ThrowMyExceptionIfNonZero(0));

    exceptionForcer.ForceException(exceptionToForce.addr);
    REQUIRE_THROWS_AS(ThrowMyExceptionIfNonZero(0), MyException);

    exceptionForcer.UnforceException(exceptionToForce.addr);
}

TEST_CASE_METHOD(ExceptionForcerFixture, "Non constexpr exception can be forced with custom input")
{
    auto exceptoinToForce = GetExceptionInfoByFnName("ThrowWithNonConstexprInputIfNonZero(int)");
    exceptionForcer.ForceException(exceptoinToForce.addr, std::make_exception_ptr(std::runtime_error("Test")));
    REQUIRE_THROWS_AS(ThrowWithNonConstexprInputIfNonZero(0), std::runtime_error);
    exceptionForcer.UnforceException(exceptoinToForce.addr);
}
