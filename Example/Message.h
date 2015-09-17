#include <stdint.h>

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
	char moreValues[2];
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
