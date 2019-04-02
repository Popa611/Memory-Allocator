/*
	REQUIRED STD:C++17 for inline static members as required in interface
	(so that the user does not have to define those static members in .cpp file
		and so that this header can be included multiple times)

	This allocator supports only 32bit or 64bit systems.

	This implementation uses the "free list" concept. The free list is a doubly linked list and
	the memory is gradually divided into chunks based on what size the user needs. The heap keeps 
	a head pointer to the first chunk in the free list. Each chunk has a header which contains
	a size of space for user data in bytes. A chunk in the free list also has to contain 2 pointers - 
	one pointing to the next chunk and one to the previous chunk in the free list. These pointers are
	overwritten by user data when removed from the free list and returned to the user.

	When the user requests memory, a proper chunk is found using the best-fit approach and removed from the free list.
	A pointer to the chunk + 8 bytes is returned to the user. The first 8 bytes of the chunk is the header containing
	the size (+ padding in case of x86 systems so that the returned address is 8 bytes aligned).

	When the user wants to deallocate memory, we compute the beginning of the chunk by subtracting 8 bytes from the
	passed pointer. Then the chunk is inserted into the free list into the proper spot (based on addresses). This is
	useful for merging adjacent chunks in the free list when they are right next to each other in the actual memory.
	This merging minimizes fragmentation.

	This implementation on x86 is a little bit slower than the std allocator tested on matrix operations according to microbenchmarks using std::chrono.
	However on x64 it looks like it is a little bit faster on the same matrix operations.

	When no suitable chunk exists during allocation -> bad_alloc exception is thrown.
	When deallocating already free memory -> undefined behaviour.

*/
#ifndef inblock_allocator_hpp
#define inblock_allocator_hpp

#include <cstddef>
#include <cstdint>

// Chunk header
struct Block
{
	std::size_t size;
	Block* next;
	Block* previous;
};

// We align returned addresses to word - 8 bytes
using word = uint64_t;

template<typename T, typename HeapHolder>
class inblock_allocator 
{
public:
	typedef T value_type;
	
	T* allocate(std::size_t n)
	{
		std::size_t size = get_aligned_size(n*sizeof(T));	// Needed size for user data
		Block* best = find_best_fit(size);

		if (!best)	// No suitable chunk found
			throw std::bad_alloc();

		remove_and_split(best, size);
		best->size = size;
		return reinterpret_cast<T*>(reinterpret_cast<char*>(best) + HeapHolder::heap.m_headerSize);	// We return header+8 -> pointer to memory for user data
	}

	void deallocate(T* ptr, std::size_t n)
	{
		Block* block = reinterpret_cast<Block*>(reinterpret_cast<char*>(ptr) - HeapHolder::heap.m_headerSize);
		insert_into_free_list(block);
		merge_adjacent_blocks(block);
	}

	// Necessary named requierements
	inblock_allocator() = default;
	template <typename U, typename Holder> 
	constexpr inblock_allocator(const inblock_allocator<U, Holder>&) noexcept { }

	template <typename U, typename Holder>
	bool operator==(const inblock_allocator<U,Holder>&) { return true; }
	template <typename U, typename Holder>
	bool operator!=(const inblock_allocator<U, Holder>&) { return false; }

private:
	std::size_t get_aligned_size(std::size_t size)
	{
		return (size+HeapHolder::heap.m_padding + sizeof(word) - 1) & ~(sizeof(word) - 1);
	}

	// Returns the minimum sized block, which can accommodate the data
	Block* find_best_fit(std::size_t size)
	{
		Block* it = HeapHolder::heap.m_listHead;
		Block* best = nullptr;	// Stays nullptr if no big enough chunk exists
		size += HeapHolder::heap.m_padding;

		// Find first fitting
		while(it)
		{
			if (it->size > size + HeapHolder::heap.m_headerSize)
			{
				best = it;
				it = it->next;
				break;
			}
			it = it->next;
		}

		// Try to find the best
		while (it)
		{
			if ((it->size > size + HeapHolder::heap.m_headerSize) && it->size < best->size)
			{
				best = it;
			}
			it = it->next;
		}
		return best;
	}

	// Removes block from the free list and adds remainder if necessary
	void remove_and_split(Block* block, std::size_t needed_size)
	{
		// If the block size is exactly what we need -> simply remove
		if (block->size == needed_size)
		{
			// It's somewhere in middle
			if (block->previous && block->next)
			{
				block->previous->next = block->next;
				block->next->previous = block->previous;
			}
			else if (block->previous)	// It's at the end
			{
				block->previous->next = nullptr;
			}
			else if (block->next)	// At the beginning
			{
				HeapHolder::heap.m_listHead = block->next;
				block->next->previous = nullptr;
			}
			else
			{
				HeapHolder::heap.m_listHead = nullptr;
			}
		}
		else	// If the block is bigger, we split it and add a new chunk to free list
		{
			Block* remainder = reinterpret_cast<Block*>(reinterpret_cast<char*>(block) + needed_size + HeapHolder::heap.m_headerSize);
			remainder->size = block->size - needed_size - HeapHolder::heap.m_headerSize;

			if (block->previous && block->next)
			{
				block->previous->next = remainder;
				block->next->previous = remainder;

				remainder->next = block->next;
				remainder->previous = block->previous;
			}
			else if (block->previous)
			{
				block->previous->next = remainder;
				remainder->previous = block->previous;
				remainder->next = nullptr;
			}
			else if (block->next)
			{
				HeapHolder::heap.m_listHead = remainder;
				block->next->previous = remainder;
				remainder->next = block->next;
				remainder->previous = nullptr;
			}
			else
			{
				remainder->next = nullptr;
				remainder->previous = nullptr;
				HeapHolder::heap.m_listHead = remainder;
			}
		}
	}

	// Inserts block to the right place in the free list
	void insert_into_free_list(Block* block)
	{
		// Find a place to insert the block
		Block* it = HeapHolder::heap.m_listHead;
		Block* prev = nullptr;
		while (it) 
		{
			/*if (block == it)
			{
				throw AlreadyDeallocatedEx();
			}
			else*/
			if (block < it) 
			{
				block->next = it;
				block->previous = prev;

				if (prev)
					prev->next = block;
				else
					HeapHolder::heap.m_listHead = block;

				it->previous = block;

				break;
			}
			prev = it;
			it = it->next;
		}

		// If the block should be at the end of the list
		if (!it && prev)
		{
			prev->next = block;
			block->previous = prev;
			block->next = nullptr;
		}
		else if (!it)	// If the free list was empty to begin with
		{
			HeapHolder::heap.m_listHead = block;
			block->next = nullptr;
			block->previous = nullptr;
		}

	}

	// Merges adjacent free blocks if there are any to avoid fragmentation
	void merge_adjacent_blocks(Block* block)
	{
		// Adjacent next block is free
		if (block->next && block->next == reinterpret_cast<Block*>(reinterpret_cast<char*>(block) + HeapHolder::heap.m_headerSize + block->size))
		{
			block->size = block->size + block->next->size + HeapHolder::heap.m_headerSize;
			block->next = block->next->next;
			if (block->next)
				block->next->previous = block;
		}

		// Adjacent previous block is free
		if (block->previous && block == reinterpret_cast<Block*>(reinterpret_cast<char*>(block->previous) + HeapHolder::heap.m_headerSize + block->previous->size))
		{
			block->previous->size = block->size + block->previous->size + HeapHolder::heap.m_headerSize;
			block->previous->next = block->next;
			if (block->next)
				block->next->previous = block->previous;
			if (!block->previous->previous)
				HeapHolder::heap.m_listHead = block->previous;
		}
	}
};

class inblock_allocator_heap
{
	template<typename T, typename HeapHolder>
	friend class inblock_allocator;

private:
	inline static  std::size_t m_bytes;	// Heap size in bytes
	inline static Block* m_heapPtr;	// Start of the heap
	inline static Block* m_listHead;	// Head of the free list
	inline constexpr static std::size_t m_headerSize = 8;	// sizeof(size_t) + (8 - sizeof(size_t)); that is if size_t is maximally 64bit
	inline constexpr static std::size_t m_padding = sizeof(void*) == 8 ? 8 : 0;	// We need to pad properly on x64 bit systems - this means that the allocator supports
																					// only 32bit or 64bit systems
public:
	void operator()(void* ptr, std::size_t n_bytes)
	{
		// I assume that the ptr is already divisible by 8. If I could not assume it I would have to add extra if condition adding some padding.
		m_bytes = n_bytes;
		m_heapPtr = reinterpret_cast<Block*>(ptr);
		m_heapPtr->next = nullptr;
		m_heapPtr->previous = nullptr;
		m_heapPtr->size = n_bytes - m_headerSize;	// size indicates only bytes free for data (without header overhead)
		m_listHead = m_heapPtr;
	}
};

#endif