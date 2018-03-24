#pragma once

#include <eforce/BuiltinConstant.h>
#include <eforce/CompiletimeRegistry.h>

namespace eforce
{
	using GenExceptionPtrFnPtr_t  = std::exception_ptr(*)();

	/// Information to store for exception forcing
	struct ThrowInfo
	{
		constexpr ThrowInfo(
			void* throwAddr,
			char const* file,
			int line,
			char const* exceptionStr,
			GenExceptionPtrFnPtr_t const& fn)
			: throwAddr(throwAddr)
			, file(file)
			, line(line)
			, exceptionStr(exceptionStr)
			, GetException(fn)
		{}

		/// Approximate address of throw
		void* const throwAddr;
		/// File containing throw
		char const* const file;
		/// Line containing throw
		int const line;
		/// Stringized exception constructor
		char const* const exceptionStr;

		/**
		 * @brief Gets an exception ptr which mimics the exception thrown from this 
		 *   location. May be null in the case that the exception constructor params 
		 *   are not constexpr.
		 */
		GenExceptionPtrFnPtr_t const& GetException;
	};


	/**
	 * @brief Helper union to cast a lambda to our GenExceptionPtrFnPtr_t. Given that we only
	 *   use this in scenarios where we've guaranteed that the lambda in question takes no 
	 *   input we can safely cast this way. This union helps us get around the casting rules
	 *   in constexpr contexts. See reasoning in the THROW_REGISTERED_EXCEPTION_HELPER macro
	 */
	template <typename T>
	union LambdaCastHelper
	{
		decltype(&T::operator()) memFn;
		GenExceptionPtrFnPtr_t ptr;

		constexpr LambdaCastHelper()
			: memFn(&T::operator())
		{ }

		constexpr LambdaCastHelper(GenExceptionPtrFnPtr_t ptr)
			: ptr(ptr) {}
	};
} // namespace eforce

#define THROW_CAT_HELPER(a, b) a ## b
#define THROW_CAT(a, b) THROW_CAT_HELPER(a, b)
#define UNIQUE_THROW_LABEL(__counter) THROW_CAT(THROW_LABEL_START, __counter) 

#define THROW_REGISTERED_EXCEPTION_HELPER(__counter, __etype, ...) do { \
	UNIQUE_THROW_LABEL(__counter): \
  /* 
   * We capture by reference here, but as long as IS_CONSTEXPR is doing it's job
   * we won't actually try to use anything unless all passed in arguments are
   * constexpr. This means it's safe to capture by reference because either 
   * 1. It's a constant expression stored in a static memory location (safe to
   * access) 
   * 2. It's somewhere else and we won't try.
   * Because of this it's also safe to cast operator() from the lambda into a
   * regular function pointer, which we do with a little union trickery.
   */ \
		static auto genExceptionFn = [&] { return std::make_exception_ptr(__etype(__VA_ARGS__)); }; \
		static constexpr auto __fn = (IS_CONSTEXPR(true, ##__VA_ARGS__)) \
			? ::eforce::LambdaCastHelper<decltype(genExceptionFn)>() \
			: ::eforce::LambdaCastHelper<decltype(genExceptionFn)>(nullptr); \
		static constexpr ::eforce::ThrowInfo throwInfo( \
			&&UNIQUE_THROW_LABEL(__counter), __FILE__, __LINE__, #__etype "(" #__VA_ARGS__ ")", __fn.ptr); \
		COMPILETIME_REGISTER(&throwInfo, "throw_locations" ); \
		throw __etype(__VA_ARGS__); \
	} while(0)

/**
 * @brief Throws an exception, but first registers it for exception forcing later.
 *   If all inputs are constexpr we will be able to throw this exception later
 *   without any user input. If they are not constexpr we will need a little help
 *   populating the exception
 * @param[in] __etype The type of exception to throw
 * @param[in] vaargs Constructor arguments for __etype
 */
#define THROW_REGISTERED_EXCEPTION(__etype, ...) \
	THROW_REGISTERED_EXCEPTION_HELPER(__COUNTER__, __etype, ##__VA_ARGS__)
