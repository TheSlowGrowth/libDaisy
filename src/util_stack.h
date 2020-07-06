#pragma once

#include <stdint.h>

namespace daisy
{

/** A simple FILO (stack) buffer with a fixed size (usefull when allocation
	on the heap is not an option */
template< typename T, int kBufferSize>
class Stack
{
public:
	Stack(): buffer_head_(0) {}
	~Stack() {}

	/** Adds an element to the back of the buffer, returning true on
		success */
	bool pushBack(const T& element_to_add)
	{
		if (!isFull())
		{
			buffer_[buffer_head_++] = element_to_add;
			return true;
		}
		return false;
	}

	/** removes and returns an element from the back of the buffer */
	T popBack()
	{
		if (isEmpty())
			return T();
		else
		{
			return buffer_[buffer_head_--];
		}
	}

	/** clears the buffer */
	void clear()
	{
		buffer_head_ = 0;
	}

	/** returns an element at the given index without checking for the
		index to be within range. */
	T& operator[](uint32_t idx) { return buffer_[idx]; }
	/** returns an element at the given index without checking for the
		index to be within range. */
	const T& operator[](uint32_t idx) const { return buffer_[idx]; }

	/** removes a single element from the buffer and returns true if successfull */
	bool remove(uint32_t idx)
	{
		if (idx >= buffer_head_)
			return false;

		for (uint32_t i = idx; i < buffer_head_ - 1; i++)
		{
			buffer_[i] = buffer_[i + 1];
		}
		buffer_head_--;
		return true;
	}

	/** removes all elements from the buffer for which
		(buffer(index) == element) returns true and returns the number of
		elements that were removed. */
	int removeAll(const T& element)
	{
		int num_removed = 0;
		int idx = buffer_head_ - 1;
		while (idx >= 0)
		{
			if (buffer_[idx] == element)
			{
				num_removed++;
				remove(idx);
				// was that the last element?
				if (idx == buffer_head_)
					idx--;
			}
			else
				idx--;
		}
		return num_removed;
	}

	/** adds a single element to the buffer and returns true if successfull */
	bool insert(uint32_t idx, const T& item)
	{
		if (buffer_head_ >= kBufferSize)
			return false;
		if (idx > buffer_head_)
			return false;
		if (idx == buffer_head_)
		{
			buffer_[buffer_head_++] = item;
			return true;
		}

		for (uint32_t i = buffer_head_ - 1; i >= idx; i--)
		{
			buffer_[i + 1] = buffer_[i];
		}
		buffer_[idx] = item;
		buffer_head_++;
		return true;
	}

	/** returns true, if the buffer is empty */
	bool isEmpty() const
	{
		return buffer_head_ == 0;
	}

	/** returns true, if the buffer is Full */
	bool isFull() const
	{
		return buffer_head_ == kBufferSize;
	}

	/** returns the number of elements in the buffer */
	uint32_t getNumElements() const
	{
		return buffer_head_;
	}

private:
	T buffer_[kBufferSize];
	uint32_t buffer_head_;
};

}
