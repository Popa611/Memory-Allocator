#ifndef inblock_allocator_hpp
#define inblock_allocator_hpp

#include <cstdint>

class inblock_allocator_heap 
{
	template<typename T, typename HeapHolder>
	friend class inblock_allocator;

private:
	static  std::size_t n_bytes_;
	static void* ptr_;

public:
	void operator()(void* ptr, std::size_t n_bytes) 
	{  
		n_bytes_ = n_bytes;
		ptr_ = ptr;
	}
};

std::size_t inblock_allocator_heap::n_bytes_ = 0;
void* inblock_allocator_heap::ptr_ = nullptr;

template<typename T, typename HeapHolder>
class inblock_allocator 
{
public:
	typedef T value_type;
	
	T* allocate(std::size_t n)
	{
		return nullptr;
	}

	void deallocate(T* ptr, std::size_t n)
	{

	}

	inblock_allocator() = default;
	template <typename U, typename Holder> 
	constexpr inblock_allocator(const inblock_allocator<U, Holder>&) noexcept {}

	template <typename U, typename Holder>
	bool operator==(const inblock_allocator<U,Holder>&) { return true; }
	template <typename U, typename Holder>
	bool operator!=(const inblock_allocator<U, Holder>&) { return false; }
};

#endif