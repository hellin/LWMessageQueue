#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <stdint.h>
#include <thread>
#include <vector>
#include "LWMessageQueue.h"
#include "TestUtils.h"

using namespace TestUtils;

namespace {

struct Message1 {
	uint32_t value;
};

struct Message2 {
	char charValue;
	uint32_t uintValue;
};

struct Message3 {
	char* stringRef;
	uint32_t uintValue;
	double doubleValue;
};

union MessageUnion {
	Message1 message1;
	Message2 message2;
	Message3 message3;
};

enum class MessageType {
	Message1,
	Message2,
	Message3
};

} // namespace

void pushMessageTest() {
	TEST_ENTER;

	using MessageQueue = LWMessageQueue::LWMessageQueue<1, 1, MessageUnion, MessageType>;
	std::unique_ptr<MessageQueue> messageQueue(new MessageQueue());

	MessageQueue::ThreadChannelOutput channelOutput = messageQueue->getThreadChannelOutput(0);
	MessageQueue::ThreadChannelInput channelInput = messageQueue->getThreadChannelInput(0);
	
	TEST_VERIFY(channelOutput.getNumMessages() == 0);

	Message1 message;
	message.value = 0;
	channelInput.pushMessage(message, MessageType::Message1);

	TEST_VERIFY(channelOutput.getNumMessages() == 1);
}

void popMessageTest() {
	TEST_ENTER;

	using MessageQueue = LWMessageQueue::LWMessageQueue<1, 1, MessageUnion, MessageType>;
	std::unique_ptr<MessageQueue> messageQueue(new MessageQueue());

	MessageQueue::ThreadChannelOutput channelOutput = messageQueue->getThreadChannelOutput(0);
	MessageQueue::ThreadChannelInput channelInput = messageQueue->getThreadChannelInput(0);

	Message2 message;
	message.charValue = 3;
	message.uintValue = 5;
	channelInput.pushMessage(message, MessageType::Message2);

	MessageQueue::MessageContainer messageContainer = channelOutput.popMessage();
	TEST_VERIFY(channelOutput.getNumMessages() == 0);

	TEST_VERIFY(messageContainer.getType() == MessageType::Message2);
	const Message2& poppedMessage = messageContainer.getMessage<Message2>();
	TEST_VERIFY(poppedMessage.charValue == 3);
	TEST_VERIFY(poppedMessage.uintValue == 5);
}

void isFullTest() {
	TEST_ENTER;

	using MessageQueue = LWMessageQueue::LWMessageQueue<2, 2, MessageUnion, MessageType>;
	std::unique_ptr<MessageQueue> messageQueue(new MessageQueue());

	MessageQueue::ThreadChannelOutput channel0Output = messageQueue->getThreadChannelOutput(0);
	MessageQueue::ThreadChannelInput channel0Input = messageQueue->getThreadChannelInput(0);
	MessageQueue::ThreadChannelOutput channel1Output = messageQueue->getThreadChannelOutput(1);
	MessageQueue::ThreadChannelInput channel1Input = messageQueue->getThreadChannelInput(1);

	Message1 message;
	message.value = 0;
	channel0Input.pushMessage(message, MessageType::Message1);
	channel1Input.pushMessage(message, MessageType::Message1);

	TEST_VERIFY(!channel0Input.isFull());
	TEST_VERIFY(!channel1Input.isFull());

	channel0Input.pushMessage(message, MessageType::Message1);
	channel1Input.pushMessage(message, MessageType::Message1);

	TEST_VERIFY(channel0Input.isFull());
	TEST_VERIFY(channel1Input.isFull());

	MessageQueue::MessageContainer messageContainer = channel0Output.popMessage();
	messageContainer = channel0Output.popMessage();
	messageContainer = channel1Output.popMessage();
	messageContainer = channel1Output.popMessage();

	TEST_VERIFY(!channel0Input.isFull());
	TEST_VERIFY(!channel1Input.isFull());
}

namespace MultiThreadTest {

const uint32_t queueSize = 1048576;
const uint32_t numInputThreads = 10;
using MessageQueue = LWMessageQueue::LWMessageQueue<queueSize, numInputThreads, MessageUnion, MessageType>;

void inputThreadEntry(MessageQueue::ThreadChannelInput inThreadChannelInput, const uint32_t inQueueSize, const uint32_t inChannelIndex) {
	Message1 message1;
	Message2 message2;

	for (uint32_t i = 0; i < inQueueSize; i += 2) {
		message1.value = inChannelIndex;
		message2.uintValue = inChannelIndex;
		message2.charValue = static_cast<char>(inChannelIndex);
		inThreadChannelInput.pushMessage(message1, MessageType::Message1);
		inThreadChannelInput.pushMessage(message2, MessageType::Message2);
	}
}

void verifyMessage(MessageQueue::MessageContainer&& inMessageContainer, const uint32_t inChannelIndex) {
	switch (inMessageContainer.getType()) {
	case MessageType::Message1:
		{
			const auto& message = inMessageContainer.getMessage<Message1>();
			TEST_VERIFY(message.value == inChannelIndex);
		}
		break;
	case MessageType::Message2:
		{
			const auto& message = inMessageContainer.getMessage<Message2>();
			TEST_VERIFY(message.uintValue == inChannelIndex && message.charValue == static_cast<char>(inChannelIndex));
		}
		break;
	default:
		TEST_VERIFY(inMessageContainer.getType() == MessageType::Message1 || inMessageContainer.getType() == MessageType::Message2);
		break;
	}
}

void outputThreadEntry(std::shared_ptr<MessageQueue> inMessageQueue, const uint32_t inQueueSize, const uint32_t inNumInputThreads) {
	try {
		std::vector<MessageQueue::ThreadChannelOutput> channelOutputs;
		channelOutputs.reserve(inNumInputThreads);

		for (uint32_t channelIndex = 0; channelIndex < inNumInputThreads; ++channelIndex) {
			channelOutputs.push_back(inMessageQueue->getThreadChannelOutput(channelIndex));
		}

		const uint32_t totalMessages = inQueueSize * inNumInputThreads;
		uint32_t receivedMessages = 0;

		while (receivedMessages < totalMessages) {
			for (uint32_t channelIndex = 0; channelIndex < inNumInputThreads; ++channelIndex) {
				MessageQueue::ThreadChannelOutput& channelOutput = channelOutputs[channelIndex];
				if (channelOutput.getNumMessages() != 0) {
					verifyMessage(channelOutput.popMessage(), channelIndex);
					++receivedMessages;
				}
			}
		}
		std::cout << "   Output thread received " << receivedMessages << " messages" << std::endl;
	} catch (const TestFailure& exception) {
		std::cout << "Test failed: " << exception.getInfo() << std::endl;
	}
}

void multiThreadTest() {
	TEST_ENTER;

	std::shared_ptr<MessageQueue> messageQueue(new MessageQueue());

	std::vector<std::unique_ptr<std::thread>> inputThreads;
	inputThreads.reserve(numInputThreads);

	for (uint32_t channelIndex = 0; channelIndex < numInputThreads; ++channelIndex) {
		inputThreads.emplace_back(new std::thread(inputThreadEntry, messageQueue->getThreadChannelInput(channelIndex), queueSize, channelIndex));
	}

	std::unique_ptr<std::thread> outputThread(new std::thread(outputThreadEntry, messageQueue, queueSize, numInputThreads));

	for (auto& inputThread : inputThreads) {
		inputThread->join();
	}
	outputThread->join();
}

} // namespace MultiThreadTest

int main(int, char**) {
	try {
		pushMessageTest();
		popMessageTest();
		isFullTest();
		MultiThreadTest::multiThreadTest();
	}
	catch (const TestFailure& exception) {
		std::cout << "Test failed: " << exception.getInfo() << std::endl;
		return 1;
	}

	return 0;
}
