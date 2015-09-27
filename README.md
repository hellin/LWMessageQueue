# LightWeightMessageQueue

A static size message queue used to send messages from many input threads to a single output thread. Input and output operations are thread safe and wait-free as long as there is exactly one thread consuming messages. Messages are user defined POD type structs.

##How to use 

Messages are passed through ThreadChannels. Each thread producing messages (input thread) gets one ThreadChannelInput instance each, and the single consumer (output) thread gets one ThreadChannelOutput instance per input thread. A good design pattern could be to let the consumer thread own the LWMessageQueue instance, and only pass ThreadChannelInput instances to the producer threads. If you need more consumers, simply create an LWMessageQueue instance per consumer thread.

It is up to the user to make sure not to push messages to a full channel. The channel (SIZE) must be dimensioned so that it never overflows. In debug builds, an assert will be hit if the channel is full when pushing new messages.

It is also up to the user to never pop messages from an empty channel. This should be done by first getting the number of pending messages from the output channel and then popping exactly that many messages. This also makes sure the output thread will finish popping messages. In debug builds, an assert will be hit if the channel is empty when popping a message.

Messages are defined as POD type structs, and a union of those structs is passed as a template parameter (MESSAGE) to LWMessageQueue. Push and pop operations copy the message data, so structs should be small enuough so that this still is a cheap operation.

Messages of any type (from the MESSAGE union) can be pushed to an input channel. When popping messages you get an LWMessageQueue<>::MessageContainer instance. To get the actual message from the container, first call messageContainer.isOfType<TYPE>() to determine the type, and then call messageContainer.getMessage<TYPE>() to get the message data cast to the correct POD type struct.

See Example/Message.h and Example/example.cpp for more details on how to use LWMessageQueue and how to define messages.

## Dependencies
Uses the following C++11 features that can be removed if someone needs to use this with an older compiler:
* std::atomic<> - Replace with OS specific atomic or gcc extension __sync_fetch_and_add()
* Deleted functions (=delete) - Replace with empty implementation instead {}
* constexpr - An optimization. Simply remove the constexpr keyword
