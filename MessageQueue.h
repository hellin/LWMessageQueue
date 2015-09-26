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
		messageContainer.isOfType<TYPE>() to determine the type, and then call messageContainer.getMessage<TYPE>() 
		to get the message data cast to the correct POD type struct.

		See the Example/Message.h and Example/example.cpp for more details on how to use LWMessageQueue and how to 
		define messages.

		Template parameters:
		SIZE is the number of allowed pending messages in one channel.
		CHANNELS is the number of channels, i.e. the number of input/producer threads.
		MESSAGE should be a union of all available Message types. Message types are POD type structs with message 
		specific data fields. See example message definitions and usage in Example/Message.h and Example/example.cpp.
*/
template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
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
		constexpr const T& getMessage() const;

		/** Check if a message container contains a message of a specific type. */
		template<typename T>
		constexpr bool isOfType() const;

	private:
		uintptr_t typeId;
		MESSAGE message;
		friend class LWMessageQueue<SIZE, CHANNELS, MESSAGE>;
	};

	/** Each input thread has its own ThreadChannelInput instance. Use it to push messages to the queue. */
	class ThreadChannelInput {
	public:
		ThreadChannelInput(ThreadChannel& inThreadChannel);

		/** Check if the channel is full. No more messages can be pushed. Should only be used for debugging purposes.
			The MessageQueue should always be dimensioned so that this does not happen.
		*/
		bool isFull() const;

		/** Push a message to the channel. The user must make sure the channel is not full before calling. Only 
			one thread may push messages to a single channel.
		*/
		template<typename T>
		void pushMessage(const T& inMessage);

		const ThreadChannelInput& operator=(const ThreadChannelInput& other);
	private:
		ThreadChannel& threadChannel;
	};

	/** The single output thread has one ThreadChannelOutput instance per input thread. Use them to pop messages
		from the queue. 
	*/
	class ThreadChannelOutput {
	public:
		ThreadChannelOutput(ThreadChannel& inThreadChannel);

		/** Get number of pending messages in the channel. The output thread should get this number and then pop
			exactly that many messages from the channel. This way it is guaranteed that an empty channel is not 
			popped and that the output thread receive loop finishes.
		*/
		uint32_t getNumMessages() const;

		/** Pop the next message from the channel. The user must make sure that the channel is not empty before 
			calling. Only one thread may pop messages from all channels.

			@return A MessageContainer. Check type by calling messageContainer.isOfType<MESSAGE_TYPE>(); 
			Then get the message data with correct type by calling messageContainer.getMessage<MESSAGE_TYPE>();
		*/
		MessageContainer popMessage();

		const ThreadChannelOutput& operator=(const ThreadChannelOutput& other);
	private:
		ThreadChannel& threadChannel;
	};

	LWMessageQueue();

	/** Get a thread channel input for an input thread. */
	ThreadChannelInput getThreadChannelInput(const uint32_t inChannel);

	/** Get a thread channel output for the output thread. */
	ThreadChannelOutput getThreadChannelOutput(const uint32_t inChannel);

private:
	class ThreadChannel {
	public:
		ThreadChannel();
		uint32_t size() const;
		void pushBack(const MessageContainer& inElement);
		MessageContainer popFront();

	private:
		MessageContainer elements[SIZE];
		uint32_t readPoint;
		uint32_t writePoint;
		std::atomic<uint32_t> numElements;

		ThreadChannel(const ThreadChannel&) = delete;
		const ThreadChannel& operator=(const ThreadChannel&) = delete;
	};

	ThreadChannel threadChannels[CHANNELS];

	LWMessageQueue(const LWMessageQueue&) = delete;
	const LWMessageQueue& operator=(const LWMessageQueue&) = delete;
};

namespace Internal {

/** Each type generates its own template instance of this function. Therefore the pointer to the static variable 
	typeIdOf::typeId<T> will be unique for every type and can be used as a type id. 
*/
template<typename T>
uintptr_t typeIdOf(const T&) {
	static uintptr_t typeId = reinterpret_cast<uintptr_t>(&typeId);
	return typeId;
}

} // namespace Internal

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
template<typename T>
constexpr const T& LWMessageQueue<SIZE, CHANNELS, MESSAGE>::MessageContainer::getMessage() const {
	return *(reinterpret_cast<const T*>(&message));
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
template<typename T>
constexpr bool LWMessageQueue<SIZE, CHANNELS, MESSAGE>::MessageContainer::isOfType() const {
	return Internal::typeIdOf(T()) == typeId;
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelInput::ThreadChannelInput(
	ThreadChannel& inThreadChannel)
	: threadChannel(inThreadChannel)
{
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
bool LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelInput::isFull() const {
	return (threadChannel.size() == SIZE);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
template<typename T>
void LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelInput::pushMessage(
	const T& inMessage)
{
	MessageContainer messageContainer;
	messageContainer.typeId = Internal::typeIdOf(inMessage);

	T* messageData = reinterpret_cast<T*>(&messageContainer.message);
	*messageData = inMessage;

	threadChannel.pushBack(messageContainer);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
const typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelInput& 
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelInput::operator=(
	const typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelInput& other)
{
	threadChannel = other.threadChannel;
	return *this;
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelOutput::ThreadChannelOutput(
	ThreadChannel& inThreadChannel)
	: threadChannel(inThreadChannel)
{
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
uint32_t LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelOutput::getNumMessages() const {
	return threadChannel.size();
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::MessageContainer 
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelOutput::popMessage() {
	return threadChannel.popFront();
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
const typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelOutput&
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelOutput::operator=(
	const typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelOutput& other)
{
	threadChannel = other.threadChannel;
	return *this;
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::LWMessageQueue() {
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelInput 
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::getThreadChannelInput(const uint32_t inChannel) {
	assert(inChannel < CHANNELS);
	return ThreadChannelInput(threadChannels[inChannel]);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannelOutput 
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::getThreadChannelOutput(const uint32_t inChannel) {
	assert(inChannel < CHANNELS);
	return ThreadChannelOutput(threadChannels[inChannel]);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannel::ThreadChannel()
	: readPoint(0),
	writePoint(0),
	numElements(0)
{
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
uint32_t LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannel::size() const {
	return numElements;
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
void LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannel::pushBack(
	const MessageContainer& inElement)
{
	assert(numElements < SIZE);
	assert(writePoint < SIZE);

	elements[writePoint] = inElement;
	writePoint = (writePoint + 1) % SIZE;
	numElements.fetch_add(1);
}

template<uint32_t SIZE, uint32_t CHANNELS, typename MESSAGE>
typename LWMessageQueue<SIZE, CHANNELS, MESSAGE>::MessageContainer 
LWMessageQueue<SIZE, CHANNELS, MESSAGE>::ThreadChannel::popFront() {
	assert(numElements > 0);
	assert(readPoint < SIZE);

	MessageContainer returnElement = elements[readPoint];
	readPoint = (readPoint + 1) % SIZE;
	numElements.fetch_sub(1);

	return returnElement;
}

} // namespace LWMessageQueue
