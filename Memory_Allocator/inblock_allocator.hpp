/*
	REQUIRED STD:C++17 for inline static members (so that the user does not have to define those static members in .cpp file
													and so that this header can be included multiple times)



*/
#ifndef inblock_allocator_hpp
#define inblock_allocator_hpp

#include <cstdint>

// Acts as a header before actual payload
struct Block
{
	std::size_t size;	// assuming size_t is 32bit on 32bit systems and 64bit on 64bit systems
	Block* next;
	Block* previous;
	void* padding;	// padding so that user data is 8 bytes aligned
};

// We align returned addresses to word - 8 bytes
using word = uint64_t;

class inblock_allocator_heap 
{
	template<typename T, typename HeapHolder>
	friend class inblock_allocator;

private:
	inline static  std::size_t n_bytes_;	// Heap size in bytes
	inline static Block* ptr_;	// Start of the heap
	inline static Block* head;	// Head of the free list

public:
	void operator()(void* ptr, std::size_t n_bytes) 
	{  
		n_bytes_ = n_bytes;
		ptr_ = (Block*)ptr;
		ptr_->next = nullptr;
		ptr_->previous = nullptr;
		ptr_->size = n_bytes - sizeof(Block);	// size indicates only bytes free for data (without header overhead)
		head = ptr_;
	}
};

template<typename T, typename HeapHolder>
class inblock_allocator 
{
public:
	typedef T value_type;
	
	T* allocate(std::size_t n)
	{
		std::size_t size = get_aligned_size(n*sizeof(T));	// Needed size for user data
		Block* best = find_best_fit(size+sizeof(Block));	// Find the best-fit block (which can accommodate both payload and header

		remove_and_split(best, size);

		best->size = size;

		return (T*)((char*)best + sizeof(Block));	// We return header+sizeof(Block) -> pointer to memory for user data
	}

	void deallocate(T* ptr, std::size_t n)
	{
		
	}

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
		return (size + sizeof(word) - 1) & ~(sizeof(word) - 1);
	}

	// Returns the minimum sized block, which can accommodate the data
	Block* find_best_fit(std::size_t size)
	{
		Block* it = HeapHolder::heap.head;
		Block* best = it;

		while (it)
		{
			if (it->size >= size && it->size < best->size)
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
				HeapHolder::heap.head = block->next;
				block->next->previous = nullptr;
			}
			else
			{
				// nothing to do (the free list remains empty because we have nothing to add)
			}
		}
		else	// If the block is bigger, we split it and add a new chunk to free list
		{
			Block* remainder = (Block*)((char*)block + needed_size + sizeof(Block));
			remainder->size = block->size - needed_size - sizeof(Block);

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
				HeapHolder::heap.head = remainder;
				block->next->previous = remainder;
				remainder->next = block->next;
				remainder->previous = nullptr;
			}
			else
			{
				remainder->next = nullptr;
				remainder->previous = nullptr;
				HeapHolder::heap.head = remainder;
			}
		}
	}
};

#endif