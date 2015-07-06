#include <assert.h>
#include <thread>
#include "MessageQueue.h"

const uint32_t numChannels = 2;
const uint32_t numMessages = 1000;

using MessageQueue = LWMessageQueue::MessageQueue<1024, numChannels>;

void inputThreadRun(MessageQueue::ThreadChannelInput inChannel) {
	for (uint32_t i = 0; i < numMessages; ++i) {
		MessageQueue::Message message;
		message.type = LWMessageQueue::User::MessageType::Message1;
		message.data.message1.value = i;

		assert(!inChannel.isFull());
		inChannel.pushMessage(message);
	}
	fprintf(stdout, "Input thread done, sent %u messages.\n", numMessages);
}

void outputThreadRun(MessageQueue* messageQueue) {
	assert(messageQueue != nullptr);
	MessageQueue::ThreadChannelOutput channel0 = messageQueue->getThreadChannelOutput(0);
	MessageQueue::ThreadChannelOutput channel1 = messageQueue->getThreadChannelOutput(1);

	uint32_t receivedMessages = 0;
	while (receivedMessages < (numMessages * numChannels)) {
		// Channel 0
		{
			const uint32_t pendingMessages = channel0.getNumMessages();
			for (uint32_t messageIndex = 0; messageIndex < pendingMessages; ++messageIndex) {
				MessageQueue::Message message = channel0.popMessage();
				++receivedMessages;
			}
		}

		// Channel 1
		{
			const uint32_t pendingMessages = channel1.getNumMessages();
			for (uint32_t messageIndex = 0; messageIndex < pendingMessages; ++messageIndex) {
				MessageQueue::Message message = channel1.popMessage();
				++receivedMessages;
			}
		}
	}

	fprintf(stdout, "Output thread done, received %u messages\n", receivedMessages);
}

int main(int argc, char** argv) {
	MessageQueue messageQueue;

	std::thread outputThread(outputThreadRun, &messageQueue);
	std::thread inputThread0(inputThreadRun, messageQueue.getThreadChannelInput(0));
	std::thread inputThread1(inputThreadRun, messageQueue.getThreadChannelInput(1));

	inputThread0.join();
	inputThread1.join();
	outputThread.join();

	return 0;
}
