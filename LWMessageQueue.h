/*
The MIT License (MIT)

Copyright (c) 2015 Marcus Spangenberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <assert.h>
#include <atomic>
#include <stdint.h>

namespace LWMessageQueue {

/**
	@brief
		A static size message queue used to send messages from many input threads to a single output thread. Input 
		and output operations are thread safe and wait-free as long as there is exactly one thread consuming messages. 
		Messages are user defined POD type structs.

	@details
		Messages are passed through ThreadChannels. Each thread producing messages (input thread) gets one 
		ThreadChannelInput instance each, and the single consumer (output) thread gets one ThreadChannelOutput instance
		per input thread. A good design pattern could be to let the consumer thread own the LWMessageQueue instance, 
		and only pass ThreadChannelInput instances to the producer threads. If you need more consumers, simply create 
		an LWMessageQueue instance per consumer thread.

		It is up to the user to make sure not to push messages to a full channel. The channel (SIZE) must be 
		dimensioned so that it never overflows. In debug builds, an assert will be hit if the channel is full when 
		pushing new messages.

		It is also up to the user to never pop messages from an empty channel. This should be done by first 
		getting the number of pending messages from the output channel and then popping exactly that many messages. 
		This also makes sure the output thread will finish popping messages. In debug builds, an assert will be hit 
		if the channel is empty when popping a message.

		Messages are defined as POD type structs, and a union of those structs is passed as a template parameter 
		(MESSAGE) to LWMessageQueue. Push and pop operations copy the message data, so structs should be small enuough
		so that this still is a cheap operation.

		Messages of any type (from the MESSAGE union) can be pushed to an input channel. When popping messages you get 
		an LWMessageQueue<>::MessageContainer instance. To get the actual message from the container, first call 
		messageContainer.getType() to determine the type, comparing it with the supplied TYPES enum values, and then 
		call messageContainer.getMessage<TYPE>() to get the message data cast to the correct POD type struct.

		See the Example/Message.h and Example/example.cpp for more details on how to use LWMessageQueue and how to 
		define messages.

		Template parameters:
		SIZE is the number of allowed pending messages in one channel.
		CHANNELS is the number of channels, i.e. the number of input/producer threads.
		MESSAGE should be a union of all available Message types. Message types are POD type structs with message 
		specific data fields. See example message definitions and usage in Example/Message.h and Example/example.cpp.
		TYPES should be a enum class with one entry per message type. See Example/Message.h and Example/example.cpp for
		types definition.
*/
template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
class LWMessageQueue {
private:
	class ThreadChannel;
public:
	/** Used for message storage in the queue. When you pop a message from an output channel, you get instances
		of this type.
	*/
	class MessageContainer {
	public:
		/** Get a reference to the message data, as the correct message type. */
		template<typename T>
		inline const T& getMessage() const noexcept;

		/** Check if a message container contains a message of a specific type. */
		inline TYPES getType() const noexcept;

	private:
		TYPES type;
		MESSAGE message;
		friend class LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>;
	};

	/** Each input thread has its own ThreadChannelInput instance. Use it to push messages to the queue. */
	class ThreadChannelInput {
	public:
		ThreadChannelInput(ThreadChannel& inThreadChannel) noexcept;
		ThreadChannelInput(const ThreadChannelInput& other) = default;
		ThreadChannelInput& operator=(const ThreadChannelInput& other) = default;

		/** Check if the channel is full. No more messages can be pushed. Should only be used for debugging purposes.
			The MessageQueue should always be dimensioned so that this does not happen.
		*/
		inline bool isFull() const noexcept;

		/** Push a message to the channel. The user must make sure the channel is not full before calling. Only 
			one thread may push messages to a single channel.
			@param inMessage Message data from the MESSAGE union.
			@param inType Message type from the TYPES enum.
		*/
		template<typename T>
		void pushMessage(const T& inMessage, const TYPES inType) noexcept;
	private:
		ThreadChannel& threadChannel;
	};

	/** The single output thread has one ThreadChannelOutput instance per input thread. Use them to pop messages
		from the queue. 
	*/
	class ThreadChannelOutput {
	public:
		ThreadChannelOutput(ThreadChannel& inThreadChannel) noexcept;
		ThreadChannelOutput(const ThreadChannelOutput& other) = default;
		ThreadChannelOutput& operator=(const ThreadChannelOutput& other) = default;

		/** Get number of pending messages in the channel. The output thread should get this number and then pop
			exactly that many messages from the channel. This way it is guaranteed that an empty channel is not 
			popped and that the output thread receive loop finishes.
		*/
		inline uint32_t getNumMessages() const noexcept;

		/** Pop the next message from the channel. The user must make sure that the channel is not empty before 
			calling. Only one thread may pop messages from all channels.

			@return A MessageContainer. Check type by calling messageContainer.getType(); 
			Then get the message data with correct type by calling messageContainer.getMessage<MESSAGE_TYPE>();
		*/
		inline MessageContainer popMessage() noexcept;

	private:
		ThreadChannel& threadChannel;
	};

	LWMessageQueue() = default;
	~LWMessageQueue() = default;

	LWMessageQueue(const LWMessageQueue&) = delete;
	LWMessageQueue& operator=(const LWMessageQueue&) = delete;
	LWMessageQueue(const LWMessageQueue&&) = delete;
	LWMessageQueue& operator=(const LWMessageQueue&&) = delete;

	/** Get a thread channel input for an input thread. */
	ThreadChannelInput getThreadChannelInput(const uint32_t inChannel) noexcept;

	/** Get a thread channel output for the output thread. */
	ThreadChannelOutput getThreadChannelOutput(const uint32_t inChannel) noexcept;

private:
	class ThreadChannel {
	public:
		ThreadChannel() noexcept;
		~ThreadChannel() = default;

		ThreadChannel(const ThreadChannel&) = delete;
		const ThreadChannel& operator=(const ThreadChannel&) = delete;
		ThreadChannel(const ThreadChannel&&) = delete;
		const ThreadChannel& operator=(const ThreadChannel&&) = delete;

		inline uint32_t size() const noexcept;
		void pushBack(const MessageContainer& inElement) noexcept;
		MessageContainer popFront() noexcept;

	private:
		MessageContainer elements[SIZE];
		uint32_t readPoint;
		uint32_t writePoint;
		std::atomic<uint32_t> numElements;
	};

	ThreadChannel threadChannels[CHANNELS];
	static constexpr uint32_t sizeMinusOne = SIZE - 1;
};

namespace Internal {

constexpr bool isPowerOfTwo(const uint32_t value) {
	return (value != 0) && ((value & (value - 1)) == 0);
}

} // namespace Internal

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
template<typename T>
inline const T& LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::MessageContainer::getMessage() const noexcept {
	return *(reinterpret_cast<const T*>(&message));
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
inline TYPES LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::MessageContainer::getType() const noexcept {
	return type;
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelInput::ThreadChannelInput(
	ThreadChannel& inThreadChannel) noexcept
	: threadChannel(inThreadChannel)
{
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
inline bool LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelInput::isFull() const noexcept {
	return (threadChannel.size() == SIZE);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
template<typename T>
void LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelInput::pushMessage(
	const T& inMessage,
	const TYPES type) noexcept
{
	static_assert(sizeof(T) <= sizeof(MESSAGE), "Type T might not be part of union MESSAGE. Size mismatch.");
	static_assert(alignof(MESSAGE) % alignof(T) == 0, "Type T might not be part of union MESSAGE. Alignment mismatch.");
	MessageContainer messageContainer;
	messageContainer.type = type;

	T* messageData = reinterpret_cast<T*>(&messageContainer.message);
	*messageData = inMessage;

	threadChannel.pushBack(messageContainer);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelOutput::ThreadChannelOutput(
	ThreadChannel& inThreadChannel) noexcept
	: threadChannel(inThreadChannel)
{
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
inline uint32_t LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelOutput::getNumMessages() const noexcept {
	return threadChannel.size();
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
inline typename LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::MessageContainer 
LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelOutput::popMessage() noexcept {
	return threadChannel.popFront();
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
typename LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelInput 
LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::getThreadChannelInput(const uint32_t inChannel) noexcept {
	assert(inChannel < CHANNELS);
	return ThreadChannelInput(threadChannels[inChannel]);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
typename LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannelOutput 
LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::getThreadChannelOutput(const uint32_t inChannel) noexcept {
	assert(inChannel < CHANNELS);
	return ThreadChannelOutput(threadChannels[inChannel]);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannel::ThreadChannel() noexcept
	: readPoint(0),
	writePoint(0),
	numElements(0)
{
	static_assert(Internal::isPowerOfTwo(SIZE), "Template parameter SIZE must be a power of two.");
	assert(numElements.is_lock_free());
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
inline uint32_t LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannel::size() const noexcept {
	return numElements;
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
void LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannel::pushBack(
	const MessageContainer& inElement) noexcept
{
	assert(numElements < SIZE);
	assert(writePoint < SIZE);

	elements[writePoint] = inElement;
	writePoint = (writePoint + 1) & sizeMinusOne; 
	numElements.fetch_add(1);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE, typename TYPES>
typename LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::MessageContainer 
LWMessageQueue<SIZE, CHANNELS, MESSAGE, TYPES>::ThreadChannel::popFront() noexcept {
	assert(numElements > 0);
	assert(readPoint < SIZE);

	MessageContainer returnElement = elements[readPoint];
	readPoint = (readPoint + 1) & sizeMinusOne;
	numElements.fetch_sub(1);

	return returnElement;
}

} // namespace LWMessageQueue
