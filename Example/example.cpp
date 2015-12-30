#include <assert.h>
#include <thread>
#include "LWMessageQueue.h"
#include "Message.h"

const uint32_t numChannels = 2;
const uint32_t numMessages = 1000;
const uint32_t numMessagesPerThread = numMessages * 2;
const uint32_t queueSize = 2048;

using MessageQueue = LWMessageQueue::LWMessageQueue<queueSize, numChannels, MessageUnion, MessageType>;

void inputThread0Run(MessageQueue::ThreadChannelInput inChannel) {
	for (uint32_t i = 0; i < numMessages; ++i) {
		{
			Message1 message;
			message.value = 17;
			message.anotherValue = 4711;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message, MessageType::Message1);
		}

		{
			Message2 message;
			message.value = 0;
			message.anotherValue = 0;
			message.moreValues[0] = 0;
			message.moreValues[1] = 0;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message, MessageType::Message2);
		}
	}
	fprintf(stdout, "Input thread 0 done, sent %u messages.\n", numMessagesPerThread);
}

void inputThread1Run(MessageQueue::ThreadChannelInput inChannel) {
	for (uint32_t i = 0; i < numMessages; ++i) {
		{
			Message1 message;
			message.value = 17;
			message.anotherValue = 4711;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message, MessageType::Message1);
		}

		{
			Message2 message;
			message.value = 1;
			message.anotherValue = 1;
			message.moreValues[0] = 1;
			message.moreValues[1] = 1;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message, MessageType::Message2);
		}
	}
	fprintf(stdout, "Input thread 1 done, sent %u messages.\n", numMessagesPerThread);
}

void verifyMessage(const uint32_t channelIndex, const MessageQueue::MessageContainer& messageContainer) {
	switch (messageContainer.getType()) {
	case MessageType::Message1:
		{
			const Message1& message = messageContainer.getMessage<Message1>();
			assert(message.value == 17);
			assert(message.anotherValue == 4711);
		}
		break;
	case MessageType::Message2:
		{
			const Message2& message = messageContainer.getMessage<Message2>();
			assert(message.value == channelIndex);
			assert(message.anotherValue == channelIndex);
			assert(message.moreValues[0] == channelIndex);
			assert(message.moreValues[1] == channelIndex);
		}
		break;
	default:
		assert(false);
		break;
	}
}

void outputThreadRun(MessageQueue* messageQueue) {
	assert(messageQueue != nullptr);
	MessageQueue::ThreadChannelOutput channel0 = messageQueue->getThreadChannelOutput(0);
	MessageQueue::ThreadChannelOutput channel1 = messageQueue->getThreadChannelOutput(1);

	uint32_t receivedMessages = 0;
	const uint32_t totalTestWantedMessages = numMessagesPerThread * numChannels;

	while (receivedMessages < totalTestWantedMessages) {
		// Channel 0
		{
			const uint32_t pendingMessages = channel0.getNumMessages();
			for (uint32_t messageIndex = 0; messageIndex < pendingMessages; ++messageIndex) {
				MessageQueue::MessageContainer messageContainer = channel0.popMessage();
				++receivedMessages;
				verifyMessage(0, messageContainer);
			}
		}

		// Channel 1
		{
			const uint32_t pendingMessages = channel1.getNumMessages();
			for (uint32_t messageIndex = 0; messageIndex < pendingMessages; ++messageIndex) {
				MessageQueue::MessageContainer messageContainer = channel1.popMessage();
				++receivedMessages;
				verifyMessage(1, messageContainer);
			}
		}
	}

	fprintf(stdout, "Output thread done, received %u messages\n", receivedMessages);
}

int main(int, char**) {
	MessageQueue messageQueue;

	std::thread outputThread(outputThreadRun, &messageQueue);
	std::thread inputThread0(inputThread0Run, messageQueue.getThreadChannelInput(0));
	std::thread inputThread1(inputThread1Run, messageQueue.getThreadChannelInput(1));

	inputThread0.join();
	inputThread1.join();
	outputThread.join();

	return 0;
}
