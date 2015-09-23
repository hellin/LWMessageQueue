#pragma once

#include <array>
#include <assert.h>
#include <atomic>
#include <stdint.h>

namespace LWMessageQueue {

/**
	@brief
		A static size message queue used to send messages from many input threads to a single output thread. Input 
		and output operations are thread safe and wait-free as long as there is exactly one channel per input thread 
		and exactly one output thread.

	@details
		QUEUE_SIZE is the number of allowed pending messages in one channel.
		NUM_CHANNELS is the number of channels, i.e. the number of input threads.
		MESSAGE_UNION is a union of all available Message types. Message types are POD type structs with message 
		specific data fields. See example message definitions and usage in Example/Message.h and Example/example.cpp.

		The input threads should each get their own ThreadChannelInput instance to push messages from that input 
		thread. The single output thread should get one ThreadChannelOutput instance per input thread to be able 
		to pop messages from each input thread.

		It is up to the user to make sure not to push messages to a full channel. The channel (QUEUE_SIZE) must be 
		dimensioned so that it never overflows. In debug builds, an assert will be hit if the channel is full when 
		pushing new messages.

		It is also up to the user to never pop messages from an empty channel. This should be done by first 
		getting the number of pending messages from the output channel and then popping exactly that many messages. 
		This also makes sure the output thread will finish popping messages. In debug builds, an assert will be hit 
		if the channel is empty when popping a message.

		Messages of any type (from MESSAGE_UNION) can be pushed directly. When popping messages you get a 
		LWMessageQueue<>::MessageContainer. To get the actual message from the container, first call 
		MessageContainer::isTypeOf<TYPE>() to determine the type, and then call MessageContainer::getMessage<TYPE>() 
		to get the message data.
*/
template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
class LWMessageQueue {
private:
		class ThreadChannel;
public:
	/** Used for message storage in the queue. When you pop a message from an output channel, you get instances
		of this type.
	*/
	class MessageContainer {
	public:
		/** Get a reference to the message data, in the correct message type. */
		template<typename MESSAGE>
		const MESSAGE& getMessage() const;

		/** Check if a message container contains a message of a specific type. */
		template<typename T>
		bool isOfType() const;

	private:
		uintptr_t typeId;
		MESSAGE_UNION message;
		friend class LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>;
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
		template<typename MESSAGE>
		void pushMessage(const MESSAGE& inMessage);

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
		MessageContainer elements[QUEUE_SIZE];
		uint32_t readPoint;
		uint32_t writePoint;
		std::atomic<uint32_t> numElements;

		ThreadChannel(const ThreadChannel&) = delete;
		const ThreadChannel& operator=(const ThreadChannel&) = delete;
	};

	std::array<ThreadChannel, NUM_CHANNELS> threadChannels;

	LWMessageQueue(const LWMessageQueue&) = delete;
	const LWMessageQueue& operator=(const LWMessageQueue&) = delete;
};

namespace Internal {

template<typename T>
uintptr_t typeIdOf(const T&) {
	static uintptr_t typeId = reinterpret_cast<uintptr_t>(&typeId);
	return typeId;
}

} // namespace Internal

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
template<typename MESSAGE>
const MESSAGE& LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::MessageContainer::getMessage() const {
	return *(reinterpret_cast<const MESSAGE*>(&message));
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
template<typename T>
bool LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::MessageContainer::isOfType() const {
	return Internal::typeIdOf(T()) == typeId;
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannel::ThreadChannel()
	: readPoint(0),
	writePoint(0),
	numElements(0)
{
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
uint32_t LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannel::size() const {
	return numElements;
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
void LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannel::pushBack(
	const MessageContainer& inElement)
{
	assert(numElements < QUEUE_SIZE);
	assert(writePoint < QUEUE_SIZE);

	elements[writePoint] = inElement;
	writePoint = (writePoint + 1) % QUEUE_SIZE;
	numElements.fetch_add(1);
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::MessageContainer 
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannel::popFront() {
	assert(numElements > 0);
	assert(readPoint < QUEUE_SIZE);

	MessageContainer returnElement = elements[readPoint];
	readPoint = (readPoint + 1) % QUEUE_SIZE;
	numElements.fetch_sub(1);

	return returnElement;
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelInput::ThreadChannelInput(
	ThreadChannel& inThreadChannel)
	: threadChannel(inThreadChannel)
{
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
bool LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelInput::isFull() const {
	return (threadChannel.size() == QUEUE_SIZE);
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
template<typename MESSAGE>
void LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelInput::pushMessage(
	const MESSAGE& inMessage)
{
	MessageContainer messageContainer;
	messageContainer.typeId = Internal::typeIdOf(inMessage);

	MESSAGE* messageData = reinterpret_cast<MESSAGE*>(&messageContainer.message);
	*messageData = inMessage;

	threadChannel.pushBack(messageContainer);
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
const typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelInput& 
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelInput::operator=(
	const typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelInput& other)
{
	threadChannel = other.threadChannel;
	return *this;
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelOutput::ThreadChannelOutput(
	ThreadChannel& inThreadChannel)
	: threadChannel(inThreadChannel)
{
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
uint32_t LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelOutput::getNumMessages() const {
	return threadChannel.size();
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::MessageContainer 
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelOutput::popMessage() {
	return threadChannel.popFront();
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
const typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelOutput&
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelOutput::operator=(
	const typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelOutput& other)
{
	threadChannel = other.threadChannel;
	return *this;
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::LWMessageQueue() {
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelInput 
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::getThreadChannelInput(const uint32_t inChannel) {
	assert(inChannel < NUM_CHANNELS);
	return ThreadChannelInput(threadChannels[inChannel]);
}

template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE_UNION>
typename LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::ThreadChannelOutput 
LWMessageQueue<QUEUE_SIZE, NUM_CHANNELS, MESSAGE_UNION>::getThreadChannelOutput(const uint32_t inChannel) {
	assert(inChannel < NUM_CHANNELS);
	return ThreadChannelOutput(threadChannels[inChannel]);
}

} // namespace LWMessageQueue
