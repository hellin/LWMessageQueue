#include <assert.h>
#include <thread>
#include "Message.h"
#include "MessageQueue.h"

const uint32_t numChannels = 2;
const uint32_t numMessages = 10;

using MessageQueue = LWMessageQueue::LWMessageQueue<1024, numChannels, Message>;

void inputThread0Run(MessageQueue::ThreadChannelInput inChannel) {
	for (uint32_t i = 0; i < numMessages; ++i) {
		{
			Message message;
			message.type = MessageType::Message1;
			message.data.message1.value = 17;
			message.data.message1.anotherValue = 4711;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message);
		}

		{
			Message message;
			message.type = MessageType::Message2;
			message.data.message2.value = 0;
			message.data.message2.anotherValue = 0;
			message.data.message2.moreValues[0] = 0;
			message.data.message2.moreValues[1] = 0;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message);
		}
	}
	fprintf(stdout, "Input thread 0 done, sent %u messages.\n", numMessages * 2);
}

void inputThread1Run(MessageQueue::ThreadChannelInput inChannel) {
	for (uint32_t i = 0; i < numMessages; ++i) {
		{
			Message message;
			message.type = MessageType::Message1;
			message.data.message1.value = 17;
			message.data.message1.anotherValue = 4711;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message);
		}

		{
			Message message;
			message.type = MessageType::Message2;
			message.data.message2.value = 1;
			message.data.message2.anotherValue = 1;
			message.data.message2.moreValues[0] = 1;
			message.data.message2.moreValues[1] = 1;

			assert(!inChannel.isFull());
			inChannel.pushMessage(message);
		}
	}
	fprintf(stdout, "Input thread 1 done, sent %u messages.\n", numMessages * 2);
}

void verifyMessage(const uint32_t channelIndex, const Message& message) {
	switch (message.type)
	{
	case MessageType::Message1:
		assert(message.data.message1.value == 17);
		assert(message.data.message1.anotherValue == 4711);
		break;
	case MessageType::Message2:
		assert(message.data.message2.value == channelIndex);
		assert(message.data.message2.anotherValue == channelIndex);
		assert(message.data.message2.moreValues[0] == channelIndex);
		assert(message.data.message2.moreValues[1] == channelIndex);
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
	const uint32_t totalTestWantedMessages = numMessages * numChannels * 2;

	while (receivedMessages < totalTestWantedMessages) {
		// Channel 0
		{
			const uint32_t pendingMessages = channel0.getNumMessages();
			for (uint32_t messageIndex = 0; messageIndex < pendingMessages; ++messageIndex) {
				Message message = channel0.popMessage();
				++receivedMessages;
				verifyMessage(0, message);
			}
		}

		// Channel 1
		{
			const uint32_t pendingMessages = channel1.getNumMessages();
			for (uint32_t messageIndex = 0; messageIndex < pendingMessages; ++messageIndex) {
				Message message = channel1.popMessage();
				++receivedMessages;
				verifyMessage(1, message);
			}
		}
	}

	fprintf(stdout, "Output thread done, received %u messages\n", receivedMessages);
}

int main(int argc, char** argv) {
	MessageQueue messageQueue;

	std::thread outputThread(outputThreadRun, &messageQueue);
	std::thread inputThread0(inputThread0Run, messageQueue.getThreadChannelInput(0));
	std::thread inputThread1(inputThread1Run, messageQueue.getThreadChannelInput(1));

	inputThread0.join();
	inputThread1.join();
	outputThread.join();

	return 0;
}
