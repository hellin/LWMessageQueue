#pragma once

#include <array>
#include <atomic>
#include <assert.h>
#include <stdint.h>
#include <thread>

namespace LWMessageQueue {

/**
	@brief
		A static size message queue used to send messages from many input threads to a single output thread. Input 
		and output operations are thread safe and wait-free as long as there is exactly one channel per input thread 
		and exactly one output thread.

	@details
		QUEUE_SIZE is the number of allowed pending messages in one channel.
		NUM_CHANNELS is the number of channels, i.e. the number of input threads.

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
*/
template<uint32_t QUEUE_SIZE, uint32_t NUM_CHANNELS, typename MESSAGE>
class LWMessageQueue {
public:
	class ThreadChannel {
	public:
		ThreadChannel()
			: readPoint(0),
			writePoint(0),
			numElements(0)
		{
		}

		uint32_t size() const {
			return numElements;
		}

		void pushBack(const MESSAGE& inElement)  {
			assert(numElements < QUEUE_SIZE);
			assert(writePoint < QUEUE_SIZE);

			elements[writePoint] = inElement;
			writePoint = (writePoint + 1) % QUEUE_SIZE;
			numElements.fetch_add(1);
		}

		MESSAGE popFront() {
			assert(numElements > 0);
			assert(readPoint < QUEUE_SIZE);

			MESSAGE returnElement = elements[readPoint];
			readPoint = (readPoint + 1) % QUEUE_SIZE;
			numElements.fetch_sub(1);

			return returnElement;
		}

	private:
		MESSAGE elements[QUEUE_SIZE];
		uint32_t readPoint;
		uint32_t writePoint;
		std::atomic<uint32_t> numElements;

		ThreadChannel(const ThreadChannel&) = delete;
		const ThreadChannel& operator=(const ThreadChannel&) = delete;
	};

	class ThreadChannelInput {
	public:
		ThreadChannelInput(ThreadChannel& inThreadChannel)
		: threadChannel(inThreadChannel)
		{}
		/**
			@brief
				Check if the channel is full. No more messages can be pushed. Should only be used for debugging purposes.
				The MessageQueue should always be dimensioned so that this does not happen.
		*/
		bool isFull() const {
			return (threadChannel.size() == QUEUE_SIZE);
		}
		/**
			@brief
				Push a message to the channel. The user must make sure the channel is not full before calling. Only 
				one thread may push messages to a single channel.
		*/
		void pushMessage(MESSAGE& inMessage) {
			threadChannel.pushBack(inMessage);
		}
	private:
		ThreadChannel& threadChannel;
	};

	class ThreadChannelOutput {
	public:
		ThreadChannelOutput(ThreadChannel& inThreadChannel)
		: threadChannel(inThreadChannel)
		{}
		/**
			@brief
				Get number of pending messages in the channel. The output thread should get this number and then pop
				exactly that many messages from the channel. This way it is guaranteed that an empty channel is not 
				popped and that the output thread receive loop finishes.
		*/
		uint32_t getNumMessages() const {
			return threadChannel.size();
		}
		/**
			@brief
				Pop next message from the channel. The user must make sure that the channel is not empty before 
				calling. Only one thread may pop messages from all channels.
		*/
		MESSAGE popMessage() {
			return threadChannel.popFront();
		}
	private:
		ThreadChannel& threadChannel;
	};

	static const uint32_t numChannels = NUM_CHANNELS;

	LWMessageQueue() {
	}

	/**
		@brief
			Get a thread channel for a single input thread.
	*/
	ThreadChannelInput getThreadChannelInput(const uint32_t inChannel) {
		assert(inChannel < NUM_CHANNELS);
		return ThreadChannelInput(threadChannels[inChannel]);
	}

	/**
		@brief
			Get a thread channel for the output thread.
	*/
	ThreadChannelOutput getThreadChannelOutput(const uint32_t inChannel) {
		assert(inChannel < NUM_CHANNELS);
		return ThreadChannelOutput(threadChannels[inChannel]);
	}

private:
	std::array<ThreadChannel, NUM_CHANNELS> threadChannels;

	LWMessageQueue(const LWMessageQueue&) = delete;
	const LWMessageQueue& operator=(const LWMessageQueue&) = delete;
};

} // namespace LWMessageQueue
