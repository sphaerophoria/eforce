# eforce

eforce is a c++ library to help with exception testing. eforce can force any function that can throw an exception to immediately throw instead of doing it's regular task.

## Example

Consider the following piece of code

```
void SomeFunction()
{
    if (SomeRareConditionNeverHitDuringDevelopment())
        throw std::runtime_error("Something bad has happened");
}
```

If we were to write code that used SomeFunction and ship it, we may not be able to easily reason about what happens in the rest of our application when SomeFunction() fails. Do we handle it as we expect? Do all callers catch? Do threads crash?

These are questions that in a perfect world we would be able to reason about easily, however it can be easy to miss a call stack if there are many callers across many threads. Wouldn't it be nice if we could just try it and see what happens?

Enter eforce. With a few lines we can add the ability to force an exception at runtime to ensure it's handled how we want. The above code just has to be modified to look like

```
#include <eforce/Exception.h>

#define THROW_RUNTIME_ERROR(__s) THROW_REGISTERED_EXCEPTION(std::runtime_error, __s)
void SomeFunction()
{
    if (SomeRareConditionNeverHitDuringDevelopment())
        THROW_RUNTIME_ERROR("Something bad has happened");
}
```

Now later, from GDB or from some debug ui we can do
```
#include <eforce/ExceptionForcer.h>

void ForceSomeFunctionThrow()
{
    eforce::ExceptoinForcer eforcer;
    auto exceptions = eforcer.GetExceptions();
    auto exceptionToForce = std::find_if(exceptions.begin(), exceptions.end(), 
        [] (eforce::ThrowInfo& exception) {
            return exception.parentFn.name == "SomeFunction()";
        });
    eforcer.ForceException(exceptoinToForce.addr);
}

```

Next call to `SomeFunction()` will now throw, even if `SomeRareConditionNeverHitDuringDevelopment()` returns false.

## Installation

This project depends on binutils libbfd, which in turn depends on libiberty and zlib. If your platform does not have these libraries I've put up a simple CMakeLists.txt to build them at https://github.com/sphaerophoria/build-bfd (Which I've been using for qemu testing). Once your dependencies are set up it's as easy as doing

```
mkdir build
cd build
cmake ..
make -j5
make install
```

If you do not have libbfd in a standard location you'll have to add `-I<include_prefix>` to your `CMAKE_CXX_FLAGS` and `-L<lib_prefix>` to your `CMAKE_EXE_LIINKER_FLAGS`

Unfortunately I am not familiar enough with cmake to automatically find the requested libraries. If anyone has experience here please let me know :).

## "Supported" platforms / Limitations

This library should work on linux platforms with a c++11 or later compiler.

I have personally tested this on (and written code for) several x64 machines (some which use dynamic executables and some that don't), as well as on ARM with THUMB2 and aarch64 in 64 bit mode. Given the implementation details this will probably not work on other platforms without a little manual intevention. ARM/Aarch64 support has only been tested in QEMU on my dev machine. Different arm processor charactaristics may influence how well this works since we do runtime code modification.

I've only tested with GCC 7.2 / clang 5.0 on an x64 linux dev machine. I haven't looked into what clang generates on OSX or if we could do something similar in cygwin on windows.

We definitely don't work on MSVC currently as we only support the Syustem V AMD64 ABI, not the MSVC one.

Currently does not work in shared object files.

## How it works

There are a couple difficult to solve problems here that we've had to work around

1. We need a compile time generated list of all possible exceptions. This cannot be done with the usual local static class instantiation as local statics are only initialized the first time they are hit. We need to register throws that haven't happened yet for this library to be useful
2. We need a way to force an exception to throw, even if it's been hidden behind multiple if statements

### Compiletime registry

We create a compiletime registry by getting our linker to help us out a little. The basic concept is that linux executables (ELF files) have several sections. You may have encountered .text, .data, and .bss. We can add as many sections as we want with our own names. To create a compiletime registry we insert a list of pointers to constexpr variables into our own tagged section. See CompiletimeRegistry.h for more details.

### Force an exception

To force an exception we take advantage of the fact that on most platforms

1. Pointers are passed in a register
2. Registers do not have to be preserved from the callee

This allows us to replace the start of a function with a register push of our exception_ptr and a jump to a function that throws that pointer. Since we use a jump (or branch), and not a call, it seems like instead of calling our original function we called our throw function. It acts as if the original function was never even called.

To replace the start of a function we need to generate the appropriate opcodes for our processor. This involves a specialized opcode generator for each instruction set. To generate a new one we have to read the documentation for our instruction set and manually fill in the appropriate opcodes to populate a register with an immediate value and jump to somewhere else. 

Once our opcodes are generated we can use the linux call `mprotect` to allow us to write into our exectuable section and swap out the start of our function.

## Tests
Tests are contained in the test folder and built by default. You can run them on your target platform by using ./test_prog. I would suggest running the tests as a basic sanity to ensure the strategies used by this library are valid on your platform.