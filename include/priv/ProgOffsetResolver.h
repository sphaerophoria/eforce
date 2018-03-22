#pragma once
#include <fstream>

namespace eforce
{
	class ProgOffsetResolver
	{
	public:
		ProgOffsetResolver()
		{
			std::ifstream f("/proc/self/maps");
			f >> std::hex >> m_progStartAddr; 
		}

		void* ToOffset(void* addr) const
		{
			if (addr == nullptr)
				return nullptr;

			return static_cast<char*>(addr) - reinterpret_cast<std::ptrdiff_t>(m_progStartAddr);
		}

		void* FromOffset(void* addr) const
		{
			if (addr == nullptr)
				return nullptr;

			return static_cast<char*>(addr) + reinterpret_cast<std::ptrdiff_t>(m_progStartAddr);
		}

	private:
		void* m_progStartAddr = nullptr;
	};
} // namespace eforce