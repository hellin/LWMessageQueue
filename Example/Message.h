#include <stdint.h>

// An example on how to declare messages of different types that can all be sent to 
// the same message queue.

// Declare message data structs.
//struct <Message_name>Data {
//	<type> <variable_name>;
//	...
//};

struct Message1Data {
	uint32_t value;
	uint32_t anotherValue;
};

struct Message2Data {
	uint32_t value;
	uint32_t anotherValue;
	uint8_t moreValues[2];
};
// End of message data structs.

// Add the message to the type enum.
enum class MessageType {
	//<Mesage_name>
	Message1,
	Message2
};

// Add the message data struct to the data union.
union MessageData {
	//<Message_name>Data <Message_name>;
	Message1Data message1;
	Message2Data message2;
};

struct Message {
	MessageType type;
	MessageData data;
};
