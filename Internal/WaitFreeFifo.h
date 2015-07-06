#pragma once

#include <assert.h>
#include <atomic>
#include <stdint.h>

namespace LWMessageQueue {

namespace Internal {

/**
@brief
	A single reader, single writer wait-free FIFO with S elements of type T.
@details
	T can be any primitive or trivial, non-primitive type.
*/
template<class T, uint32_t S>
class WaitFreeFifo {
public:
	WaitFreeFifo()
	: readPoint(0),
		writePoint(0),
		numElements(0)
	{
	}

	uint32_t size() const {
		return numElements;
	}

	void pushBack(const T& inElement)  {
		assert(numElements < S);
		assert(writePoint < S);

		elements[writePoint] = inElement;
		writePoint = (writePoint + 1) % S;
		numElements.fetch_add(1);
	}

	T popFront() {
		assert(numElements > 0);
		assert(readPoint < S);

		T returnElement = elements[readPoint];
		readPoint = (readPoint + 1) % S;
		numElements.fetch_sub(1);

		return returnElement;
	}

private:
	T elements[S];
	uint32_t readPoint;
	uint32_t writePoint;
	std::atomic<uint32_t> numElements;

	WaitFreeFifo(const WaitFreeFifo&) = delete;
	const WaitFreeFifo& operator=(const WaitFreeFifo&) = delete;
};

} // namespace Internal

} // namespace LWMessageQueue
